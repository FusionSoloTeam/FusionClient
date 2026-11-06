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

MACRO_CONFIG_INT(FcCoord, fc_coord, 3, 0, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show the tee's X-coordinate fraction above nameplates (wall+arrow at .44/.53): 0=none, 1=others, 2=own, 3=all")
MACRO_CONFIG_INT(FcCoordSize, fc_coord_size, 100, 20, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size in percent of the coordinate indicator drawn above nameplates")

MACRO_CONFIG_INT(FcCinematicCamEnable, fc_cinematic_cam_enable, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable cinematic ease-in/out of the camera when panning in spectate")
MACRO_CONFIG_INT(FcCinematicCam, fc_cinematic_cam, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Cinematic ease-in/out of the camera when panning in spectate (0=off, higher=smoother, slower)")

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

MACRO_CONFIG_INT(FcAboveBelowWallCheck, fc_above_below_wall_check, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only show the above/below player indicator when no wall is between you and the player")

MACRO_CONFIG_INT(FcChatLog, fc_chat_log, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Save chat messages to chatlog/ and delete logs older than a week")

MACRO_CONFIG_INT(FcScoreboardGradientTeams, fc_scoreboard_gradient_teams, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draw scoreboard team blocks and team titles with a gradient")
MACRO_CONFIG_INT(FcScoreboardSort, fc_scoreboard_sort, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Scoreboard sorting (0 = off, 1 = friends first, 2 = friends first and war last)")

MACRO_CONFIG_INT(FcChatSpecClick, fc_chat_spec_click, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Click a chat line asking for a spectate to spectate that player")

MACRO_CONFIG_INT(FcSendPosMarkers, fc_sendpos_markers, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show positions shared by other Fusion clients with !sendpos")
MACRO_CONFIG_INT(FcSendPosTime, fc_sendpos_time, 10, 1, 60, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Seconds a shared position marker stays visible")

MACRO_CONFIG_INT(FcDeathTracker, fc_death_tracker, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show where tees died on the map");
MACRO_CONFIG_INT(FcDeathTrackerOthers, fc_death_tracker_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Also mark the deaths of other players (0 = only your own tees)");
MACRO_CONFIG_INT(FcDeathTrackerTime, fc_death_tracker_time, 60, 1, 600, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Seconds a death marker stays visible");
MACRO_CONFIG_INT(FcDeathTrackerMax, fc_death_tracker_max, 32, 1, 128, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Maximum number of death markers kept on the map");
MACRO_CONFIG_COL(FcDeathTrackerColor, fc_death_tracker_color, 0xE8564FFF, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of the death markers");

MACRO_CONFIG_STR(FcDummyGameSkin, fc_dummy_gameskin, 50, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Gameskin used for your own dummy (empty = same as cl_asset_game)");

MACRO_CONFIG_INT(FcModernUi, fc_modern_ui, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Modern UI variant (0 = classic, 1 = dark, 2 = white)")
MACRO_CONFIG_INT(FcModernUiPanelAlpha, fc_modern_ui_panel_alpha, 65, 10, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Modern UI panel and button translucency in percent")
MACRO_CONFIG_INT(FcModernUiAnim, fc_modern_ui_anim, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Modern UI animations (hover, split, transitions, startup window)")

MACRO_CONFIG_INT(FcEditorEnvelopeHelp, fc_editor_envelope_help, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Map editor envelopes: quick-start hints, richer tooltips and a live time-scrub preview bar")

MACRO_CONFIG_INT(FcFusionAmount, fc_fusion_amount, 20, 1, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Fusion input: how many milliseconds the tee is predicted ahead")
MACRO_CONFIG_INT(FcFusionSmoothing, fc_fusion_smoothing, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Fusion input: how slowly the ahead-offset eases in and out (0 = instant, higher = slower)")

MACRO_CONFIG_INT(FcLagSmooth, fc_lag_smooth, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Smooth the own/dummy tee when it teleports after a packet-loss stall instead of snapping to the server position")
MACRO_CONFIG_INT(FcLagSmoothStall, fc_lag_smooth_stall, 120, 30, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Lag smoothing: how long (ms) without a new game tick before a stall is considered to have happened")
MACRO_CONFIG_INT(FcLagSmoothTime, fc_lag_smooth_time, 250, 50, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Lag smoothing: how long (ms) to ease the tee from its last shown position to the server position after a stall")
MACRO_CONFIG_INT(FcLagSmoothThreshold, fc_lag_smooth_threshold, 32, 4, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Lag smoothing: minimum position jump (world units) on stall recovery to treat as a teleport worth smoothing")
MACRO_CONFIG_INT(FcLagSmoothExtrap, fc_lag_smooth_extrap, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Lag smoothing: while stalled and the tee has frozen, glide it forward on its last velocity")

MACRO_CONFIG_INT(FcCrosshair, fc_crosshair, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draw a dynamic crosshair at your aim point")
MACRO_CONFIG_INT(FcCrosshairShape, fc_crosshair_shape, 1, 0, 7, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dynamic crosshair shape (0 = ring, 1 = cross, 2 = X, 3 = reticle, 4 = square, 5 = diamond, 6 = triangle, 7 = dot)")
MACRO_CONFIG_INT(FcCrosshairFilled, fc_crosshair_filled, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Fill the polygon crosshair shapes (ring, square, diamond, triangle) solid instead of outlining them")
MACRO_CONFIG_COL(FcCrosshairColor, fc_crosshair_color, 0xFFFFFFFF, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Dynamic crosshair color")
MACRO_CONFIG_INT(FcCrosshairSize, fc_crosshair_size, 80, 8, 600, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dynamic crosshair size (world units, your tee is ~64)")
MACRO_CONFIG_INT(FcCrosshairThickness, fc_crosshair_thickness, 8, 1, 80, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dynamic crosshair line thickness (world units)")
MACRO_CONFIG_INT(FcCrosshairGap, fc_crosshair_gap, 16, 0, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dynamic crosshair center gap (world units)")
MACRO_CONFIG_INT(FcCrosshairRotate, fc_crosshair_rotate, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Rotate the dynamic crosshair with your tee speed")
MACRO_CONFIG_INT(FcCrosshairRotateSpeed, fc_crosshair_rotate_speed, 200, 0, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dynamic crosshair rotation speed as a percentage of tee speed")
MACRO_CONFIG_INT(FcCrosshairPulse, fc_crosshair_pulse, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Pulse the dynamic crosshair to system audio (like tile outline pulsation)")
MACRO_CONFIG_INT(FcCrosshairPulseStrength, fc_crosshair_pulse_strength, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dynamic crosshair pulse strength")
MACRO_CONFIG_INT(FcCrosshairHideDefault, fc_crosshair_hide_default, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hide the default weapon cursor sprite while the dynamic crosshair is on")
