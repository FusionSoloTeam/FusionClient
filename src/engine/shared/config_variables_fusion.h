// This file can be included several times.

#ifndef MACRO_CONFIG_INT
#error "Do not include this file directly, use config_variables.h instead"
#endif

MACRO_CONFIG_INT(FcCustomClients, fc_custom_clients, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show custom client icons")
MACRO_CONFIG_INT(FcCustomClientsShare, fc_custom_clients_share, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Send your own client id to other players (disable to stay undetectable)")
MACRO_CONFIG_INT(FcCustomClientsDetect, fc_custom_clients_detect, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Detect custom clients of other players")

MACRO_CONFIG_INT(FcCustomClientsScoreboard, fc_custom_clients_scoreboard, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show custom client icons on the scoreboard")
MACRO_CONFIG_INT(FcCustomClientsNamePlates, fc_custom_clients_nameplates, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show custom client icons next to player name plates")
MACRO_CONFIG_INT(FcCustomClientsOwn, fc_custom_clients_own, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show the custom client icon for your own tee as well")
MACRO_CONFIG_INT(FcCustomClientsColor, fc_custom_clients_color, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tint custom client icons with the tee colors")
MACRO_CONFIG_INT(FcCustomClientsBestClient, fc_custom_clients_bestclient, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Detect BestClient users via its public user list (contacts an external server)")

MACRO_CONFIG_INT(FcSkinGradient, fc_skin_gradient, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Animated glowing gradient on tee skins")
MACRO_CONFIG_COL(FcSkinGradientColor1, fc_skin_gradient_color1, 0xBEA880FF, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Skin gradient color 1")
MACRO_CONFIG_COL(FcSkinGradientColor2, fc_skin_gradient_color2, 0xCBFFB0FF, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Skin gradient color 2")
MACRO_CONFIG_INT(FcSkinGradientMode, fc_skin_gradient_mode, 0, 0, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Skin gradient direction (0 = diagonal, 1 = vertical, 2 = horizontal, 3 = radial)")
MACRO_CONFIG_INT(FcSkinGradientSpeed, fc_skin_gradient_speed, 100, 10, 400, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Skin gradient animation speed")
MACRO_CONFIG_INT(FcSkinGradientBands, fc_skin_gradient_bands, 2, 1, 6, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Skin gradient band count")
MACRO_CONFIG_INT(FcSkinGradientIntensity, fc_skin_gradient_intensity, 60, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Skin gradient color strength")
MACRO_CONFIG_INT(FcSkinGradientGlow, fc_skin_gradient_glow, 45, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Skin gradient glow strength")
MACRO_CONFIG_INT(FcSkinGradientOthers, fc_skin_gradient_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply the skin gradient to other players too")
MACRO_CONFIG_INT(FcSkinGradientFeet, fc_skin_gradient_feet, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply the skin gradient to the tee feet as well as the body")

MACRO_CONFIG_INT(FcAnimatedAssets, fc_animated_assets, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Animate .gif files dropped into the skins and assets folders")
MACRO_CONFIG_INT(FcAnimatedAssetsFps, fc_animated_assets_fps, 15, 1, 60, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Maximum frames per second used to upload animated assets")
MACRO_CONFIG_INT(FcAnimatedAssetsSpeed, fc_animated_assets_speed, 100, 10, 400, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Animated assets playback speed in percent")
MACRO_CONFIG_INT(FcAnimatedAssetsGame, fc_animated_assets_game, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Animate game, particles, emoticons, hud and extras assets loaded from .gif files")
MACRO_CONFIG_INT(FcAnimatedAssetsVideoAlpha, fc_animated_assets_video_alpha, 1, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Alpha key for video assets: 0 = opaque, 1 = treat black as transparent, 2 = treat white as transparent")
MACRO_CONFIG_INT(FcAnimatedAssetsVideoFrames, fc_animated_assets_video_frames, 64, 1, 240, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Maximum number of frames decoded from a video asset")

MACRO_CONFIG_INT(FcOutlineUnderCursor, fc_outline_under_cursor, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Outline the tile the cursor is aiming at in-game")
MACRO_CONFIG_COL(FcOutlineUnderCursorColor, fc_outline_under_cursor_color, 0xFFFFFFFF, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of the tile outline under the cursor")
MACRO_CONFIG_INT(FcOutlineUnderCursorWidth, fc_outline_under_cursor_width, 2, 1, 8, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Thickness of the tile outline under the cursor")

MACRO_CONFIG_INT(FcTileAnimations, fc_tile_animations, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Animate tiles when they enter the camera")
MACRO_CONFIG_INT(FcTileAnimationMode, fc_tile_animation_mode, 1, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tile entry animation (0 = glitched entities, 1 = fade, 2 = placing)")
MACRO_CONFIG_INT(FcTileAnimationSpeed, fc_tile_animation_speed, 100, 10, 400, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tile entry animation speed in percent")
MACRO_CONFIG_INT(FcTileAnimationPlacingDistance, fc_tile_animation_placing_distance, 6, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tile placing animation distance in tiles")
MACRO_CONFIG_INT(FcTileAnimationGlitchInterval, fc_tile_animation_glitch_interval, 45, 10, 250, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Entity glitch animation frame interval in milliseconds")
