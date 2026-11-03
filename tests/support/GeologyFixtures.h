#pragma once

#include "domains/natural/geology/catalog/GeologyCatalog.h"
#include "domains/natural/planetary/PlanetMap.h"
#include "support/MaterialFixtures.h"

// Shared by the geology test files: the catalog and planet every test reads.
namespace InfinitesiaTests
{
    inline const GeologyCatalog& SharedCatalog()
    {
        static const GeologyCatalog catalog = GeologyCatalog::Load(SharedMaterials(), kColumnsFile);
        return catalog;
    }

    inline const PlanetMap& SharedMap()
    {
        static const PlanetMap map = []
        {
            PlanetParameters parameters;
            parameters.latitudeBands = 90;
            parameters.longitudeSegments = 180;
            return PlanetMap(parameters);
        }();
        return map;
    }
}
