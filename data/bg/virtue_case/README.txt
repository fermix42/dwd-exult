Compact native case presentation requested by owner:196x135.
Original full-resolution supplied image:source_case_reference.png.
Deterministic importer from repository root:
python tools/virtue_case/compact_art.py --source data/bg/virtue_case/source_case_reference.png
Rebuild SHP from the current14 indexed PNGs in this directory:
python asset_tools.py --input . --output converted
Compare converted/virtue_case.shp with ../virtue_case.shp.
Current manifest supersedes the old320x200 contract; prior assets retained in Git.
Closed source PNG remains28x20. The manifest packs only frame0 at14x10,
origin7,9, using center-sampled nearest-neighbor palette indices. Inventory and
world both use that real shape1601/frame0. No scene-specific scaling is applied.
All other frames remain unchanged. Stones13x15, lid buttons32x12, native tiny font.
No spell or label-editing behavior is implemented by importing these images.
