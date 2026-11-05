#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "presentation/ui/PlanetGlobeGeometry.h"
#include "presentation/ui/PlanetPalette.h"
#include "presentation/ui/PlanetViewer.h"
#include "presentation/ui/PlanetViewerLayout.h"
#include "raylib.h"
#include "raymath.h"

namespace Infinitesia
{
namespace
{
constexpr int kColorBufferIndex = 3; // Mesh::vboId slot for vertex colors
}

void PlanetViewer::EnsureSurfaceTexture()
{
	if (m_surfaceTexture.IsValid())
		return;

	const int rows = m_planetMap.GetLatitudeBandCount();
	const int columns = m_planetMap.GetLongitudeSegmentCount();
	Image surfaceImage = GenImageColor(columns, rows, BLACK);
	if (surfaceImage.data == nullptr)
		throw std::runtime_error("Failed to allocate the planet surface map image.");
	auto *surfacePixels = static_cast<Color *>(surfaceImage.data);
	for (int latitude = 0; latitude < rows; ++latitude)
	{
		for (int longitude = 0; longitude < columns; ++longitude)
		{
			const std::size_t regionIndex = static_cast<std::size_t>(latitude) * columns + longitude;
			surfacePixels[static_cast<std::size_t>(rows - 1 - latitude) * columns + longitude] =
			    m_regionColors[regionIndex];
		}
	}
	UniqueTexture texture(LoadTextureFromImage(surfaceImage));
	UnloadImage(surfaceImage);
	if (!texture.IsValid())
		throw std::runtime_error("Failed to create the planet surface map texture.");
	SetTextureFilter(texture.Get(), TEXTURE_FILTER_BILINEAR);
	m_surfaceTexture = std::move(texture);
}

void PlanetViewer::WriteCellColors(Color *out, int row, int column, bool selected) const
{
	const int columns = m_planetMap.GetLongitudeSegmentCount();
	const std::size_t topLeft = static_cast<std::size_t>(row) * (columns + 1) + column;
	const std::size_t topRight = topLeft + 1;
	const std::size_t bottomLeft = topLeft + columns + 1;
	const std::size_t bottomRight = bottomLeft + 1;

	Color tl = kSelectedCellColor;
	Color tr = kSelectedCellColor;
	Color bl = kSelectedCellColor;
	Color br = kSelectedCellColor;
	if (!selected)
	{
		tl = ShadeColor(m_sphereVertexColors[topLeft], m_sphereVertexBrightness[topLeft]);
		tr = ShadeColor(m_sphereVertexColors[topRight], m_sphereVertexBrightness[topRight]);
		bl = ShadeColor(m_sphereVertexColors[bottomLeft], m_sphereVertexBrightness[bottomLeft]);
		br = ShadeColor(m_sphereVertexColors[bottomRight], m_sphereVertexBrightness[bottomRight]);
	}
	// Same triangle order as the former immediate-mode path.
	out[0] = tl;
	out[1] = bl;
	out[2] = tr;
	out[3] = tr;
	out[4] = bl;
	out[5] = br;
}

void PlanetViewer::SyncGlobeMesh() const
{
	const int rows = m_planetMap.GetLatitudeBandCount();
	const int columns = m_planetMap.GetLongitudeSegmentCount();
	const int selectedCell = (rows - 1 - m_selectedLatitude) * columns + m_selectedLongitude;

	if (m_globeMesh.vaoId == 0)
	{
		const int cellCount = rows * columns;
		Mesh mesh{};
		mesh.vertexCount = cellCount * 6;
		mesh.triangleCount = cellCount * 2;
		mesh.vertices = static_cast<float *>(RL_MALLOC(static_cast<std::size_t>(mesh.vertexCount) * 3 * sizeof(float)));
		mesh.colors =
		    static_cast<unsigned char *>(RL_MALLOC(static_cast<std::size_t>(mesh.vertexCount) * sizeof(Color)));
		if (mesh.vertices == nullptr || mesh.colors == nullptr)
		{
			RL_FREE(mesh.vertices);
			RL_FREE(mesh.colors);
			throw std::runtime_error("Failed to allocate the globe mesh.");
		}

		auto *colors = reinterpret_cast<Color *>(mesh.colors);
		float *vertices = mesh.vertices;
		for (int row = 0; row < rows; ++row)
		{
			for (int column = 0; column < columns; ++column)
			{
				const int cell = row * columns + column;
				const std::size_t topLeft = static_cast<std::size_t>(row) * (columns + 1) + column;
				const Vector3 tl = m_sphereVertices[topLeft];
				const Vector3 tr = m_sphereVertices[topLeft + 1];
				const Vector3 bl = m_sphereVertices[topLeft + columns + 1];
				const Vector3 br = m_sphereVertices[topLeft + columns + 2];
				const Vector3 cellVertices[6] = {tl, bl, tr, tr, bl, br};
				float *dst = vertices + static_cast<std::size_t>(cell) * 18;
				for (const Vector3 &vertex : cellVertices)
				{
					*dst++ = vertex.x;
					*dst++ = vertex.y;
					*dst++ = vertex.z;
				}
				WriteCellColors(colors + static_cast<std::size_t>(cell) * 6, row, column, cell == selectedCell);
			}
		}

		UploadMesh(&mesh, true);
		if (mesh.vaoId == 0)
		{
			RL_FREE(mesh.vertices);
			RL_FREE(mesh.colors);
			throw std::runtime_error("Failed to upload the globe mesh.");
		}
		m_globeMesh = mesh;
		m_globeMaterial = LoadMaterialDefault();
		m_uploadedSelectedCell = selectedCell;
		return;
	}

	if (selectedCell == m_uploadedSelectedCell)
		return;

	auto *colors = reinterpret_cast<Color *>(m_globeMesh.colors);
	const auto rewriteCell = [&](int cell, bool selected) {
		Color *cellColors = colors + static_cast<std::size_t>(cell) * 6;
		WriteCellColors(cellColors, cell / columns, cell % columns, selected);
		UpdateMeshBuffer(m_globeMesh, kColorBufferIndex, cellColors, static_cast<int>(6 * sizeof(Color)),
		                 static_cast<int>(static_cast<std::size_t>(cell) * 6 * sizeof(Color)));
	};
	if (m_uploadedSelectedCell >= 0)
		rewriteCell(m_uploadedSelectedCell, false);
	rewriteCell(selectedCell, true);
	m_uploadedSelectedCell = selectedCell;
}

void PlanetViewer::ReleaseRenderResources() noexcept
{
	if (m_globeMesh.vaoId != 0)
	{
		UnloadMesh(m_globeMesh);
		UnloadMaterial(m_globeMaterial);
		m_globeMesh = {};
		m_globeMaterial = {};
		m_uploadedSelectedCell = -1;
	}
	m_surfaceTexture.Reset();
	m_regionMap.ReleaseRenderResources();
	m_worldMapView.ReleaseRenderResources();
}

Matrix PlanetViewer::GetGlobeTransform() const
{
	const Matrix overview =
	    MatrixMultiply(MatrixRotateX(m_tiltDegrees * DEG2RAD), MatrixRotateY(m_rotationDegrees * DEG2RAD));
	if (m_regionBlend <= 0.0f)
	{
		return overview;
	}

	const Matrix facing = MakeFacingMatrix(m_regionMap.GetFocusCenter());
	if (m_regionBlend >= 1.0f)
	{
		return facing;
	}
	return QuaternionToMatrix(
	    QuaternionSlerp(QuaternionFromMatrix(overview), QuaternionFromMatrix(facing), SmootherStepF(m_regionBlend)));
}

Camera3D PlanetViewer::MakeCamera() const
{
	const ViewerLayout layout = GetLayout(GetScreenWidth(), GetScreenHeight(), m_planetMap);
	const float fieldOfViewRadians = kCameraFieldOfView * kPi / 180.0f;
	const float focalLength = static_cast<float>(GetScreenHeight()) / (2.0f * std::tan(fieldOfViewRadians * 0.5f));
	// The globe stays a planet-scale view in both modes. Zooming it to the
	// focused region only magnified a 360x720 cell texture into a blur; the
	// region map is the close-up, and the globe gives the planet context with a
	// marker at the focused cell.
	const float cameraDistance = std::max(2.4f, focalLength / layout.globeRadius * 1.08f) * m_zoom;
	// Shift the view so the globe centre lands on the panel centre; the needed
	// shift grows with the camera distance.
	const float anchorDepth = cameraDistance;
	const float targetZ =
	    (static_cast<float>(GetScreenWidth()) * 0.5f - layout.globeCenter.x) * anchorDepth / focalLength;
	const float targetY =
	    (layout.globeCenter.y - static_cast<float>(GetScreenHeight()) * 0.5f) * anchorDepth / focalLength;

	Camera3D camera{};
	camera.position = {-cameraDistance, targetY, targetZ};
	camera.target = {0.0f, targetY, targetZ};
	camera.up = {0.0f, 1.0f, 0.0f};
	camera.fovy = kCameraFieldOfView;
	camera.projection = CAMERA_PERSPECTIVE;
	return camera;
}

} // namespace Infinitesia
