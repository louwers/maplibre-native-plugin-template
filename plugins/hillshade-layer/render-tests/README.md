# Hillshade plugin render tests

These are the complete built-in hillshade render fixtures with only the layer
type changed from `hillshade` to `org.maplibre.hillshade`. Expected images are
shared with the built-in implementation, so any pixel difference is a plugin
regression rather than a separately approved visual baseline.

`fixtures.db` is a pruned, read-only render-test resource fixture containing
the DEM, Terrarium, raster, and sprite responses used by these tests. It is
committed so the suite remains fully offline and reproducible.
