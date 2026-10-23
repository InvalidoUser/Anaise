# Fictional adult college cast

Replaced on 2026-10-23 at the user's request with entirely fictional adults.
No reference photograph, image upload, celebrity identity or real-person likeness
was used to generate this cast. The former stock-photo cast is not used by the
game or retained in the active masters.

| Character | Adult character design |
| --- | --- |
| Hana | 26-year-old East Asian woman, wavy black hair and bangs |
| Rei | 25-year-old white woman, blonde ponytail and freckles |
| Yumi | 27-year-old South Asian woman, long dark hair and oval glasses |
| Mika | 26-year-old Black woman, natural curly hair |

## Assets

`masters/NAME-expressions.png`: 1536x1024 color master, a 3-column,
2-row sheet. Cells: neutral, happy, shy / hurt, afraid, dead.
`masters/NAME-neutral.png` is a 256x256 crop of the generated neutral cell.
`NAME-faces.bmp` is a 1536x256 luminance atlas of the same six expressions,
embedded as resources 401-404. All active masters contain generated pixels only.
Source/generation metadata is stripped from checked-in PNGs.

`tools/make_school_faces.py` rebuilds the atlases from the masters with Pillow:
Lanczos resize, luminance conversion, 1% autocontrast, contrast 1.12.
The game dithers at the final destination size so world faces and dialogue
portraits remain pure black and white without resizing pre-dithered dots.
No runtime Python, downloads or loose asset files are required.

The dead-state master has no open wounds; game-code blood particles, pools,
stains and knife animation are rendered separately. All horror involves this
fictional adult college cast.

## Built-in imagegen prompts

Four separate built-in imagegen generation calls, with no reference images.
No CLI fallback was used.

### hana

```text
Use case: photorealistic-natural. Asset type: facial expression texture atlas for an adult college horror videogame. Create an ENTIRELY FICTIONAL adult character named hana: a visibly adult 26-year-old East Asian woman, shoulder-length wavy black hair and soft bangs, warm brown eyes, subtle freckles, artistic but elegant face. Invent a new face; do not depict or resemble any existing real person, celebrity, photograph, or prior character reference. Clearly mature adult facial proportions, minimum age 25, no adolescent appearance. Attractive natural photographic face, realistic pores and skin, soft studio lighting, dark neutral background, simple modest casual college shirt collar. ONE image with EXACTLY 3 equal columns and 2 equal rows, SIX aligned square head-and-neck portraits of this same invented woman. Row 1 left-to-right: neutral attentive, warm smiling happy, shy bashful gentle smile. Row 2 left-to-right: hurt/disappointed frown, extremely frightened screaming with wide eyes and open mouth, deceased with eyes closed and slack pale expression. Last cell is a fictional horror-game death-state portrait, without wounds or gore. Preserve identical face, hairstyle, hair silhouette, eyewear, skin tone, lighting, framing and scale in all six cells. Front-facing camera; crown at 6% cell height, eyes around 42%, chin at 82%; full head remains within each cell. Only head, neck and small collar. No hands, weapons, scene, text, captions, labels, borders, gutters, logos, watermark, cartoons, anime, actual real people or children. Six sharp color photographic images in one coherent 3-by-2 sheet, 3:2 landscape aspect.
```

### rei

```text
Use case: photorealistic-natural. Asset type: facial expression texture atlas for an adult college horror videogame. Create an ENTIRELY FICTIONAL adult character named rei: a visibly adult 25-year-old white woman with honey-blonde hair in a loose ponytail, green eyes, freckles, athletic and confident face. Invent a new face; do not depict or resemble any existing real person, celebrity, photograph, or prior character reference. Clearly mature adult facial proportions, minimum age 25, no adolescent appearance. Attractive natural photographic face, realistic pores and skin, soft studio lighting, dark neutral background, simple modest casual college shirt collar. ONE image with EXACTLY 3 equal columns and 2 equal rows, SIX aligned square head-and-neck portraits of this same invented woman. Row 1 left-to-right: neutral attentive, warm smiling happy, shy bashful gentle smile. Row 2 left-to-right: hurt/disappointed frown, extremely frightened screaming with wide eyes and open mouth, deceased with eyes closed and slack pale expression. Last cell is a fictional horror-game death-state portrait, without wounds or gore. Preserve identical face, hairstyle, hair silhouette, eyewear, skin tone, lighting, framing and scale in all six cells. Front-facing camera; crown at 6% cell height, eyes around 42%, chin at 82%; full head remains within each cell. Only head, neck and small collar. No hands, weapons, scene, text, captions, labels, borders, gutters, logos, watermark, cartoons, anime, actual real people or children. Six sharp color photographic images in one coherent 3-by-2 sheet, 3:2 landscape aspect.
```

### yumi

```text
Use case: photorealistic-natural. Asset type: facial expression texture atlas for an adult college horror videogame. Create an ENTIRELY FICTIONAL adult character named yumi: a visibly adult 27-year-old South Asian woman, long straight dark brown hair, fine oval eyeglasses, warm brown eyes, thoughtful elegant face. Invent a new face; do not depict or resemble any existing real person, celebrity, photograph, or prior character reference. Clearly mature adult facial proportions, minimum age 25, no adolescent appearance. Attractive natural photographic face, realistic pores and skin, soft studio lighting, dark neutral background, simple modest casual college shirt collar. ONE image with EXACTLY 3 equal columns and 2 equal rows, SIX aligned square head-and-neck portraits of this same invented woman. Row 1 left-to-right: neutral attentive, warm smiling happy, shy bashful gentle smile. Row 2 left-to-right: hurt/disappointed frown, extremely frightened screaming with wide eyes and open mouth, deceased with eyes closed and slack pale expression. Last cell is a fictional horror-game death-state portrait, without wounds or gore. Preserve identical face, hairstyle, hair silhouette, eyewear, skin tone, lighting, framing and scale in all six cells. Front-facing camera; crown at 6% cell height, eyes around 42%, chin at 82%; full head remains within each cell. Only head, neck and small collar. No hands, weapons, scene, text, captions, labels, borders, gutters, logos, watermark, cartoons, anime, actual real people or children. Six sharp color photographic images in one coherent 3-by-2 sheet, 3:2 landscape aspect.
```

### mika

```text
Use case: photorealistic-natural. Asset type: facial expression texture atlas for an adult college horror videogame. Create an ENTIRELY FICTIONAL adult character named mika: a visibly adult 26-year-old Black woman, dark skin, voluminous natural curly black hair, expressive brown eyes, elegant heart-shaped face. Invent a new face; do not depict or resemble any existing real person, celebrity, photograph, or prior character reference. Clearly mature adult facial proportions, minimum age 25, no adolescent appearance. Attractive natural photographic face, realistic pores and skin, soft studio lighting, dark neutral background, simple modest casual college shirt collar. ONE image with EXACTLY 3 equal columns and 2 equal rows, SIX aligned square head-and-neck portraits of this same invented woman. Row 1 left-to-right: neutral attentive, warm smiling happy, shy bashful gentle smile. Row 2 left-to-right: hurt/disappointed frown, extremely frightened screaming with wide eyes and open mouth, deceased with eyes closed and slack pale expression. Last cell is a fictional horror-game death-state portrait, without wounds or gore. Preserve identical face, hairstyle, hair silhouette, eyewear, skin tone, lighting, framing and scale in all six cells. Front-facing camera; crown at 6% cell height, eyes around 42%, chin at 82%; full head remains within each cell. Only head, neck and small collar. No hands, weapons, scene, text, captions, labels, borders, gutters, logos, watermark, cartoons, anime, actual real people or children. Six sharp color photographic images in one coherent 3-by-2 sheet, 3:2 landscape aspect.
```

