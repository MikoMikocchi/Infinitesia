#pragma once

#include <optional>
#include <string>
#include <vector>

#include "domains/natural/planetary/PlanetMap.h"
#include "domains/natural/planetary/StarSystem.h"
#include "domains/natural/planetary/pipeline/PlanetPipeline.h"
#include "domains/natural/planetary/region/SettlementTerrain.h"
#include "foundation/LoadProgress.h"
#include "presentation/ui/RegionMapView.h"
#include "presentation/ui/UniqueTexture.h"
#include "raylib.h"

namespace Infinitesia {
class PlanetViewer {
 public:
  explicit PlanetViewer(StarSystem system, LoadProgress* progress = nullptr);
  // The planet is built by `pipeline` (the default stages plus those of the
  // domains the game uses).
  PlanetViewer(StarSystem system, const PlanetPipeline& pipeline,
               LoadProgress* progress = nullptr);

  void Update();
  void Draw() const;
  void ReleaseRenderResources() noexcept;

  // Set when the player founds the settlement; handed over once.
  std::optional<SettlementSite> TakeFoundedSite();
  void SetFoundingEnabled(bool enabled) noexcept {
    m_foundingEnabled = enabled;
  }
  const RegionTerrain& GetRegionTerrain() const noexcept {
    return m_regionMap.GetTerrain();
  }

 private:
  void HandleInput();
  void EnsureSurfaceTexture();
  Camera3D MakeCamera() const;
  Matrix GetGlobeTransform() const;
  void SyncGlobeMesh() const;
  void WriteCellColors(Color* out, int row, int column, bool selected) const;
  void EnterRegionMode();
  void TryFoundSettlement();
  void FocusRegionOnSelectedCell();
  void SyncSelectionToRegion();

  StarSystem m_system;
  PlanetMap m_planetMap;
  RegionMapView m_regionMap;
  std::vector<Color> m_regionColors;
  std::vector<float> m_regionBrightness;
  std::vector<Vector3> m_sphereVertices;
  std::vector<Color> m_sphereVertexColors;
  std::vector<float> m_sphereVertexBrightness;
  UniqueTexture m_surfaceTexture;
  // Static globe mesh (6 vertices per cell, baked vertex colors), built lazily
  // once a GL context exists. Rotation is applied via a transform matrix.
  mutable Mesh m_globeMesh{};
  mutable Material m_globeMaterial{};
  mutable int m_uploadedSelectedCell = -1;
  int m_selectedLatitude = 0;
  int m_selectedLongitude = 0;
  float m_rotationDegrees = 0.0f;
  float m_tiltDegrees = -12.0f;
  float m_zoom = 1.0f;
  bool m_foundingEnabled = true;
  std::optional<SettlementSite> m_foundedSite;
  std::string m_notice;
  float m_noticeSeconds = 0.0f;
  bool m_regionMode = false;
  float m_regionBlend =
      0.0f;  // 0 = whole globe, 1 = zoomed onto the focused region
  bool m_draggingGlobe = false;
  Vector2 m_lastMousePosition{};
};
}  // namespace Infinitesia
