#pragma once
/* Host-Mod Breitbild 16:9 (keine Originalsemantik; video.ini widescreen = on, Debugmenue).
   Wie die Konsolen-Patches - GameCube GCDE08 "04488170 3F619368" (BHD_ASPECT_X 1.174 * 0.75),
   PS2 SLUS-20184 (Projektionswort 1.0 -> 0.75, gsaspectratio 16:9) - bleibt das Spielbild
   640x480 und wird anamorph gezeichnet: die waagerechte Projektion jeder 3D-Ansicht im Hauptbild
   schrumpft auf 3/4, die Praesentation dehnt das fertige Bild auf 16:9. Die senkrechte Sicht
   bleibt (Hor+). 2D (HUD, Menues, Texte) wird wie auf den Konsolen mitgedehnt; Filme behalten
   4:3 mit Seitenbalken (movie.cpp). Render-Texturen in der Welt (draw_screen) bleiben 4:3. */
#ifdef __cplusplus
extern "C" {
#endif
extern float recvx_widescreen_scale; /* 1.0 = 4:3 (Original), 0.75 = 16:9 */
/* Setzt den Faktor und baut NaViewScreenMatrix (njProjectScreen) neu (ninja_matrix.c). */
void recvx_set_widescreen(int on);
/* BHD_ASPECT_X (main.c:53) der 3D-Ansichten im Hauptbild. */
static inline float recvx_aspect_x(void){return 1.174f*recvx_widescreen_scale;}
#ifdef __cplusplus
}
#endif
