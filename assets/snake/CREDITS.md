# Snake portrait sources

All three original image reproductions are CC0 1.0 under The Metropolitan
Museum of Art's Open Access initiative:
https://www.metmuseum.org/hubs/open-access
https://creativecommons.org/publicdomain/zero/1.0/

Retrieved 2026-10-23. The collection API marked each `isPublicDomain: true`.
The adjacent JSON files record title, artist, object URL and download URL.

- 436532: Vincent van Gogh, Self-Portrait with a Straw Hat, 1887.
  https://www.metmuseum.org/art/collection/search/436532
- 436323: Jean Honore Fragonard, Marie Emilie Coignet de Courson with a Dog.
  https://www.metmuseum.org/art/collection/search/436323
- 437397: Rembrandt, Self-Portrait, 1660.
  https://www.metmuseum.org/art/collection/search/437397

Original JPEGs are retained as import sources. The face BMPs are cropped,
resized and converted to luminance by tools/make_snake_faces.py. Runtime
ordered dithering produces pure black and white. Eye-roll, drill-hole and
drool animation overlays are game modifications, not part of the artworks.
No runtime downloads, external libraries, or personal account data are used.
