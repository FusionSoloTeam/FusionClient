// This file can be included several times.

#ifndef MACRO_CONFIG_INT
#error "The config macros must be defined"
#define MACRO_CONFIG_INT(Tcme, ScriptName, Def, Min, Max, Save, Desc) ;
#define MACRO_CONFIG_COL(Tcme, ScriptName, Def, Save, Desc) ;
#define MACRO_CONFIG_STR(Tcme, ScriptName, Len, Def, Save, Desc) ;
#endif

#if defined(CONF_FAMILY_WINDOWS)
MACRO_CONFIG_INT(TcAllowAnyRes, tc_allow_any_res, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to allow any resolution in game when zoom is allowed (buggy on Windows)")
#else
MACRO_CONFIG_INT(TcAllowAnyRes, tc_allow_any_res, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to allow any resolution in game when zoom is allowed (buggy on Windows)")
#endif

MACRO_CONFIG_INT(TcShowChatClient, tc_show_chat_client, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show chat messages from the client such as echo")

MACRO_CONFIG_INT(TcShowFrozenText, tc_frozen_tees_text, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show how many tees in your team are currently frozen. (0 - off, 1 - show alive, 2 - show frozen)")
MACRO_CONFIG_INT(TcShowFrozenHud, tc_frozen_tees_hud, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show frozen tee HUD")
MACRO_CONFIG_INT(TcShowFrozenHudSkins, tc_frozen_tees_hud_skins, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use ninja skin, or darkened skin for frozen tees on hud")

MACRO_CONFIG_INT(TcFrozenHudTeeSize, tc_frozen_tees_size, 15, 8, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size of tees in frozen tee hud. (Default : 15)")
MACRO_CONFIG_INT(TcFrozenMaxRows, tc_frozen_tees_max_rows, 1, 1, 6, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Maximum number of rows in frozen tee HUD display")
MACRO_CONFIG_INT(TcFrozenHudTeamOnly, tc_frozen_tees_only_inteam, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only render frozen tee HUD display while in team")

MACRO_CONFIG_INT(TcNameplatePingCircle, tc_nameplate_ping_circle, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows a circle to indicate ping in the nameplate")
MACRO_CONFIG_INT(TcNameplateCountry, tc_nameplate_country, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows the country flag in the nameplate")
MACRO_CONFIG_INT(TcNameplateSkins, tc_nameplate_skins, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows skin names in nameplates, good for finding missing skins")

MACRO_CONFIG_INT(TcFlyingNamePlates, tc_flying_name_plates, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render name plates like flying kites attached to players")
MACRO_CONFIG_INT(TcFlyingNamePlatesHideLine, tc_flying_name_plates_hide_line, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hide the line between players and flying name plates")
MACRO_CONFIG_INT(TcFlyingNamePlatesLift, tc_flying_name_plates_lift, 28, 0, 120, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Extra vertical lift for flying name plates")
MACRO_CONFIG_INT(TcFlyingNamePlatesDrag, tc_flying_name_plates_drag, 52, 0, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How far flying name plates trail behind movement")
MACRO_CONFIG_INT(TcFlyingNamePlatesFollow, tc_flying_name_plates_follow, 40, 1, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How quickly flying name plates catch up to their target position")

MACRO_CONFIG_INT(TcFakeCtfFlags, tc_fake_ctf_flags, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows fake CTF flags on people (0 = off, 1 = red, 2 = blue)")

MACRO_CONFIG_INT(TcLimitMouseToScreen, tc_limit_mouse_to_screen, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Limit mouse to screen boundaries")
MACRO_CONFIG_INT(TcScaleMouseDistance, tc_scale_mouse_distance, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Improve mouse precision by scaling max distance to 1000")

MACRO_CONFIG_INT(TcHammerRotatesWithCursor, tc_hammer_rotates_with_cursor, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Allow your hammer to rotate like other weapons")

MACRO_CONFIG_INT(TcMiniVoteHud, tc_mini_vote_hud, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "When enabled makes the vote UI small")

// Anti Latency Tools
MACRO_CONFIG_INT(TcRemoveAnti, tc_remove_anti, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Removes some amount of antiping & player prediction in freeze")
MACRO_CONFIG_INT(TcUnfreezeLagTicks, tc_remove_anti_ticks, 5, 0, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "The biggest amount of prediction ticks that are removed")
MACRO_CONFIG_INT(TcUnfreezeLagDelayTicks, tc_remove_anti_delay_ticks, 25, 5, 150, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How many ticks it takes to remove the maximum prediction after being frozen")

MACRO_CONFIG_INT(TcUnpredOthersInFreeze, tc_unpred_others_in_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dont predict other players if you are frozen")
MACRO_CONFIG_INT(TcPredMarginInFreeze, tc_pred_margin_in_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable changing prediction margin while frozen")
MACRO_CONFIG_INT(TcPredMarginInFreezeAmount, tc_pred_margin_in_freeze_amount, 15, 0, 2000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Set what your prediction margin while frozen should be")

MACRO_CONFIG_INT(TcPredictTpTiles, tc_predicttptiles, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to predict teleport tiles")
MACRO_CONFIG_INT(TcPredictSoloTiles, tc_predict_solo_tiles, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to predict solo tiles so collisions with other tees are dropped before the server confirms it")
MACRO_CONFIG_INT(TcPredictLasers, tc_predict_lasers, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to predict map lasers (rotation and length of beams)")
MACRO_CONFIG_INT(TcPredictGuns, tc_predict_guns, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to predict own weapon shots even if antiping weapon prediction is disabled")

MACRO_CONFIG_INT(TcPredictionMarginAdjust, tc_prediction_margin_adjust, 0, 0, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "The maximum amount to adjust the prediction margin upwards on lag spikes (0 to disable)")

MACRO_CONFIG_INT(TcShowpredShowPeak, tc_showpred_showpeak, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to show the peak prediction time (requires cl_showpred)")
MACRO_CONFIG_INT(TcShowpredColor, tc_showpred_color, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to color the prediction time display based on the difference between the current prediction time and the average (requires cl_showpred)")
MACRO_CONFIG_INT(TcShowpredLength, tc_showpred_length, 50, 1, 50, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How long prediction times are tracked for tc_showpred_showpeak and tc_showpred_color (1/10)")
MACRO_CONFIG_INT(TcShowpredExtremePred, tc_showpred_extremepred, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Attempts to exaggerate changes in ping for tc_showpred_color (probably badly)")
MACRO_CONFIG_INT(TcShowpredWorstDiff, tc_showpred_worstdiff, 10, 1, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "The difference in prediction time that produces the most red color (requires tc_showpred_color)")

MACRO_CONFIG_INT(TcPlayerIdleAura, tc_idle_aura, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show an aura around your tee when idle")
MACRO_CONFIG_INT(TcPlayerIdleAuraTimer, tc_idle_aura_timer, 2, 2, 30, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Seconds of inactivity before the idle aura appears")
MACRO_CONFIG_INT(TcIdleAuraType, tc_idle_aura_type, 1, 1, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Idle aura style (1 = Starfall, 3 = Clock)")
MACRO_CONFIG_INT(TcToggleAuraParticlesNum, tc_toggle_aura_parsicle_num, 12, 0, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Number of idle aura particles")
MACRO_CONFIG_INT(TcToggleAuraRadius, tc_toggle_aura_radius, 30, 0, 600, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Radius of the idle aura")

MACRO_CONFIG_INT(TcHoverMessage, tc_hover_message, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show system messages as hover notifications")
MACRO_CONFIG_INT(TcHoverOnly, tc_hover_only, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show system messages only as hover notifications, not in chat")
MACRO_CONFIG_INT(TcReactBypass, tc_react_bypass, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Re-apply mute/war/heart when a listed player changes their nick")
MACRO_CONFIG_INT(TcHookCollOverlay, tc_hook_coll_overlay, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draw the hook collision line to its full length, even through tiles and tees")
MACRO_CONFIG_INT(TcChatFixLayout, tc_chat_fix_layout, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Suggest a chat command typed in a wrong keyboard layout")
MACRO_CONFIG_INT(TcAntiUnspectate, tc_anti_unspectate, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Ask for confirmation when unspeccing inside another tee")
MACRO_CONFIG_INT(TcMuteMessagesConsole, tc_mute_messages_console, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show messages from muted players in the console")
MACRO_CONFIG_INT(TcHoverMessagesHistory, tc_hover_messages_history, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show hover notification history while chat is open")
MACRO_CONFIG_INT(TcHoverMessagesMaxHistory, tc_hover_message_max_history, 15, 1, 40, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Maximum hover notification history entries")
MACRO_CONFIG_INT(TcHoverMessagesMaxNotifications, tc_hover_message_max_notifications, 10, 1, 40, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Maximum active hover notifications")
MACRO_CONFIG_INT(TcHoverPlacing, tc_hover_placing, 0, 0, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Where hover notifications appear (0 = top-right, 1 = top-left, 2 = bottom-right, 3 = bottom-left)")

MACRO_CONFIG_INT(TcAutoNotifyOnJoin, tc_auto_notify_on_join, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Notify when a player on your list joins")
MACRO_CONFIG_STR(TcAutoNotifyName, tc_auto_notify_name, 128, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Comma-separated names to notify when they join")
MACRO_CONFIG_STR(TcAutoNotifyMsg, tc_auto_notify_msg, 256, "Your Fav Person Has Joined!", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Echo message when a listed player joins (@nick is replaced with their name)")
MACRO_CONFIG_INT(TcNotifyOnMove, tc_notify_on_move, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Notify when your player is moved while the window is inactive")
MACRO_CONFIG_INT(TcNotifyOnSwitch, tc_notify_on_switch, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Notify when you press a switch button")

MACRO_CONFIG_INT(TcCursorTrail, tc_cursor_trail, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show a trail behind the ingame cursor")
MACRO_CONFIG_INT(TcCursorTrailMode, tc_cursor_trail_mode, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Cursor trail mode (0=overlay, 1=background)")
MACRO_CONFIG_INT(TcCursorTrailFade, tc_cursor_trail_fade, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Fade out the cursor trail at the end")
MACRO_CONFIG_INT(TcCursorTrailLength, tc_cursor_trail_length, 400, 50, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Cursor trail lifetime in milliseconds")
MACRO_CONFIG_INT(TcCursorTrailColorMode, tc_cursor_trail_color_mode, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Cursor trail color mode (0=static, 1=rainbow, 2=gradient)")
MACRO_CONFIG_COL(TcCursorTrailColor, tc_cursor_trail_color, 0xFFFFFFFF, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Cursor trail color")
MACRO_CONFIG_COL(TcCursorTrailColor2, tc_cursor_trail_color2, 0xAAFF80FF, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Cursor trail gradient color 2")
MACRO_CONFIG_STR(TcNotifyOnSwitchText, tc_notify_on_switch_text, 128, "Pressed button @num (@type)", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Text to output when pressing a switch button (@num = number, @type = activate/deactivate)")

MACRO_CONFIG_INT(TcHighProcessPriority, tc_high_process_priority, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Set DDNet process priority to high")
MACRO_CONFIG_INT(TcDiscordNormalProcessPriority, tc_discord_normal_process_priority, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Set Discord process priority to normal")

MACRO_CONFIG_INT(TcSpecMenuPrefixes, tc_specmenu_prefixes, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show prefixes next to names in the spectate menu")

MACRO_CONFIG_INT(TcChatScroll, tc_chat_scroll, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable scrolling in chat")

MACRO_CONFIG_INT(TcChatCursor, tc_chat_cursor, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Unlock the cursor while the chat is open")
MACRO_CONFIG_INT(TcChatTagCursor, tc_chat_tag_cursor, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tag with cursor: click a tee or a chat nickname to insert its name into the chat")
MACRO_CONFIG_INT(TcChatTagCursorHitbox, tc_chat_tag_cursor_hitbox, 200, 50, 500, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tag with cursor hitbox size in percent of the tee size")

MACRO_CONFIG_INT(TcShowOthersInMenu, tc_show_others_in_menu, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show the settings icon when a tee is in a menu")

MACRO_CONFIG_INT(TcMenuScale, tc_menu_scale, 100, 50, 150, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Scale of the menu interface in percent")

MACRO_CONFIG_INT(TcNotifyOnMoveInSpec, tc_notify_on_move_in_spec, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show when someone moves you while spectating")
MACRO_CONFIG_INT(TcTextOnMoveInSpec, tc_text_on_move_in_spec, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show text when someone moves you while spectating")
MACRO_CONFIG_INT(TcTextOnMoveInSpecPosX, tc_text_on_move_in_spec_pos_x, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Text position X when moved in spec")
MACRO_CONFIG_INT(TcTextOnMoveInSpecPosY, tc_text_on_move_in_spec_pos_y, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Text position Y when moved in spec")
MACRO_CONFIG_INT(TcTextOnMoveInSpecRemoveOnInput, tc_text_on_move_in_spec_remove_on_input, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Remove 'moved in spec' text on any input")

MACRO_CONFIG_INT(TcHideEnemyChat, tc_hide_enemy_chat, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hide chat messages of war-list enemies")
MACRO_CONFIG_INT(TcChatServerPrefix, tc_do_chat_server_prefix, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show server message prefix")
MACRO_CONFIG_INT(TcChatClientPrefix, tc_do_chat_client_prefix, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show client message prefix")
MACRO_CONFIG_INT(TcSpectatePrefix, tc_do_spec_prefix, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show spectator prefix next to names")
MACRO_CONFIG_INT(TcWarlistPrefixes, tc_warlist_prefixes, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show warlist prefix next to names")
MACRO_CONFIG_STR(TcClientPrefix, tc_client_prefix, 5, "— ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Client (echo) message prefix")
MACRO_CONFIG_STR(TcServerPrefix, tc_server_prefix, 5, "*** ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Server message prefix")
MACRO_CONFIG_STR(TcWarlistPrefix, tc_warlist_prefix, 5, "♦ ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Warlist prefix")
MACRO_CONFIG_STR(TcFriendPrefix, tc_friend_prefix, 5, "♥ ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Friend prefix")
MACRO_CONFIG_STR(TcSpecPrefix, tc_spec_prefix, 5, "• ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Spectator prefix")

MACRO_CONFIG_INT(TcShowOthersGhosts, tc_show_others_ghosts, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ghosts for other players in their unpredicted position")
MACRO_CONFIG_INT(TcSwapGhosts, tc_swap_ghosts, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show predicted players as ghost and normal players as unpredicted")
MACRO_CONFIG_INT(TcHideFrozenGhosts, tc_hide_frozen_ghosts, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hide Ghosts of other players if they are frozen")

MACRO_CONFIG_INT(TcPredGhostsAlpha, tc_pred_ghosts_alpha, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Alpha of predicted ghosts (0-100)")
MACRO_CONFIG_INT(TcUnpredGhostsAlpha, tc_unpred_ghosts_alpha, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Alpha of unpredicted ghosts (0-100)")
MACRO_CONFIG_INT(TcRenderGhostAsCircle, tc_render_ghost_as_circle, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render Ghosts as circles instead of tee")

MACRO_CONFIG_INT(TcShowCenter, tc_show_center, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws lines to show the center of your screen/hitbox")
MACRO_CONFIG_INT(TcShowCenterWidth, tc_show_center_width, 0, 0, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Center lines width (enabled by tc_show_center)")
MACRO_CONFIG_COL(TcShowCenterColor, tc_show_center_color, 1694498688, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Center lines color (enabled by tc_show_center)") // transparent red

MACRO_CONFIG_INT(TcFastInput, tc_fast_input, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Uses input for prediction before the next tick")
MACRO_CONFIG_INT(TcFastInputAmount, tc_fast_input_amount, 20, 1, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How many milliseconds fast input will apply")
MACRO_CONFIG_INT(TcFastInputOthers, tc_fast_input_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply fast input to other tees")
MACRO_CONFIG_INT(TcFastInputRepredictHook, tc_fast_input_repredict_hook, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Repredict fast input hook when the direction changes (requires Sub-Tick aiming disabled)")

MACRO_CONFIG_INT(TcAntiPingImproved, tc_antiping_improved, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Different antiping smoothing algorithm, not compatible with cl_antiping_smooth")
MACRO_CONFIG_INT(TcAntiPingNegativeBuffer, tc_antiping_negative_buffer, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Helps in Gores. Allows internal certainty value to be negative which causes more conservative prediction")
MACRO_CONFIG_INT(TcAntiPingStableDirection, tc_antiping_stable_direction, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Predicts optimistically along the tees stable axis to reduce delay in gaining overall stability")
MACRO_CONFIG_INT(TcAntiPingUncertaintyScale, tc_antiping_uncertainty_scale, 150, 25, 400, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Determines uncertainty duration as a factor of ping, 100 = 1.0")
MACRO_CONFIG_INT(TcAntiPingInteractionOnly, tc_antiping_interaction_only, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only apply antiping smoothing to players near your view")
MACRO_CONFIG_INT(TcAntiPingInteractionRadius, tc_antiping_interaction_radius, 800, 100, 4000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Radius in world units for the antiping interaction filter")
MACRO_CONFIG_INT(TcVramFallback, tc_vram_fallback, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Allocate new textures in system RAM when VRAM is full (slower, prevents out of VRAM errors)")
MACRO_CONFIG_INT(TcTextureMaxSize, tc_texture_max_size, 0, 0, 8192, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Downscale textures above this size to save VRAM (0 = off)")
MACRO_CONFIG_INT(TcVramLimit, tc_vram_limit, 0, 0, 32768, CFGFLAG_CLIENT | CFGFLAG_SAVE, "VRAM budget in MiB, allocations above it go to system RAM (0 = off)")
MACRO_CONFIG_INT(TcRamLimit, tc_ram_limit, 0, 0, 65536, CFGFLAG_CLIENT | CFGFLAG_SAVE, "System RAM budget in MiB for spilled textures and staging (0 = off)")
MACRO_CONFIG_INT(TcInfoVram, tc_info_vram, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show VRAM and RAM usage info on screen")
MACRO_CONFIG_INT(TcInfoTextures, tc_info_textures, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show loaded map textures and their sizes on screen")

MACRO_CONFIG_INT(TcColorFreeze, tc_color_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use skin colors for frozen tees")
MACRO_CONFIG_INT(TcColorFreezeDarken, tc_color_freeze_darken, 90, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Makes color of tees darker when in freeze (0-100)")
MACRO_CONFIG_INT(TcColorFreezeFeet, tc_color_freeze_feet, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Also use color for frozen tee feet")

// Revert Variables
MACRO_CONFIG_INT(TcSmoothPredictionMargin, tc_prediction_margin_smooth, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Makes prediction margin transition smooth, causes worse ping jitter adjustment (reverts a DDNet change)")
MACRO_CONFIG_INT(TcFrozenKatana, tc_frozen_katana, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show katana on frozen players (reverts a DDNet change)")
MACRO_CONFIG_INT(TcOldTeamColors, tc_old_team_colors, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use rainbow team colors (reverts a DDNet change)")
MACRO_CONFIG_INT(TcRevertHookLine, tc_revert_hook_line, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Revert hookline tip behavior")

// Outline Variables
MACRO_CONFIG_INT(TcOutline, tc_outline, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outlines")
MACRO_CONFIG_INT(TcOutlineEntities, tc_outline_in_entities, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only show outlines in entities")

MACRO_CONFIG_INT(TcOutlineSolid, tc_outline_solid, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outline around hook and unhook")
MACRO_CONFIG_INT(TcOutlineFreeze, tc_outline_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outline around freeze and deep")
MACRO_CONFIG_INT(TcOutlineUnfreeze, tc_outline_unfreeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outline around unfreeze and undeep")
MACRO_CONFIG_INT(TcOutlineKill, tc_outline_kill, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outline around kill")
MACRO_CONFIG_INT(TcOutlineTele, tc_outline_tele, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outline around teleporters")

MACRO_CONFIG_INT(TcOutlineWidthSolid, tc_outline_width_solid, 2, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Width of outline around hook and unhook")
MACRO_CONFIG_INT(TcOutlineWidthFreeze, tc_outline_width_freeze, 2, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Width of outline around freeze and deep")
MACRO_CONFIG_INT(TcOutlineWidthUnfreeze, tc_outline_width_unfreeze, 2, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Width of outline around unfreeze and undeep")
MACRO_CONFIG_INT(TcOutlineWidthKill, tc_outline_width_kill, 2, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Width of outline around kill")
MACRO_CONFIG_INT(TcOutlineWidthTele, tc_outline_width_tele, 2, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Width of outline around teleporters")

MACRO_CONFIG_COL(TcOutlineColorSolid, tc_outline_color_solid, 4294901760, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of outline around hook and unhook") // 255 0 0 0
MACRO_CONFIG_COL(TcOutlineColorFreeze, tc_outline_color_freeze, 4294901760, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of outline around freeze and deep") // 255 0 0 0
MACRO_CONFIG_COL(TcOutlineColorUnfreeze, tc_outline_color_unfreeze, 4294901760, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of outline around unfreeze and undeep") // 255 0 0 0
MACRO_CONFIG_COL(TcOutlineColorKill, tc_outline_color_kill, 4294901760, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of outline around kill") // 0 0 0
MACRO_CONFIG_COL(TcOutlineColorTele, tc_outline_color_tele, 4294901760, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of outline around teleporters") // 255 0 0 0

MACRO_CONFIG_INT(TcOutlineGradient, tc_outline_gradient, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draw tile outlines with a gradient color")
MACRO_CONFIG_COL(TcOutlineGradientColor1, tc_outline_gradient_color1, 4278255488, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Tile outline gradient color 1")
MACRO_CONFIG_COL(TcOutlineGradientColor2, tc_outline_gradient_color2, 4288282496, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Tile outline gradient color 2")
MACRO_CONFIG_INT(TcOutlineGradientSpeed, tc_outline_gradient_speed, 20, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tile outline gradient scroll speed (0=static gradient)")
MACRO_CONFIG_INT(TcOutlinePulsate, tc_outline_pulsate, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Pulse tile outlines to system audio")
MACRO_CONFIG_INT(TcOutlinePulsateStrength, tc_outline_pulsate_strength, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tile outline pulsation strength")
MACRO_CONFIG_INT(TcOutlinePulsateFilter, tc_outline_pulsate_filter, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Ignore the game's own audio and the filter list in outline pulsation")
MACRO_CONFIG_STR(TcOutlinePulsateFilterList, tc_outline_pulsate_filter_list, 128, "discord.exe,ddnet.exe", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Process names ignored by outline pulsation (comma separated)")
MACRO_CONFIG_INT(TcOutlineAlpha, tc_outline_alpha, 0, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Transparency of all tile outlines (0 = opaque, 100 = invisible)")

MACRO_CONFIG_INT(TcOutlineGradientSolid, tc_outline_gradient_solid, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Gradient outline for hook and unhook tiles")
MACRO_CONFIG_COL(TcOutlineGradientColor2Solid, tc_outline_gradient_color2_solid, 4288282496, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Gradient color 2 for hook and unhook outlines")
MACRO_CONFIG_INT(TcOutlineGradientFreeze, tc_outline_gradient_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Gradient outline for freeze and deep tiles")
MACRO_CONFIG_COL(TcOutlineGradientColor2Freeze, tc_outline_gradient_color2_freeze, 4288282496, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Gradient color 2 for freeze outlines")
MACRO_CONFIG_INT(TcOutlineGradientUnfreeze, tc_outline_gradient_unfreeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Gradient outline for unfreeze and undeep tiles")
MACRO_CONFIG_COL(TcOutlineGradientColor2Unfreeze, tc_outline_gradient_color2_unfreeze, 4288282496, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Gradient color 2 for unfreeze outlines")
MACRO_CONFIG_INT(TcOutlineGradientKill, tc_outline_gradient_kill, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Gradient outline for kill tiles")
MACRO_CONFIG_COL(TcOutlineGradientColor2Kill, tc_outline_gradient_color2_kill, 4288282496, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Gradient color 2 for kill outlines")
MACRO_CONFIG_INT(TcOutlineGradientTele, tc_outline_gradient_tele, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Gradient outline for teleporter tiles")
MACRO_CONFIG_COL(TcOutlineGradientColor2Tele, tc_outline_gradient_color2_tele, 4288282496, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Gradient color 2 for teleporter outlines")

MACRO_CONFIG_INT(TcCustomAspectRatioMode, tc_custom_aspect_ratio_mode, -1, -1, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Aspect ratio mode (-1=legacy auto, 0=off, 1=preset, 2=custom)")
MACRO_CONFIG_INT(TcCustomAspectRatioApplyMode, tc_custom_aspect_ratio_apply_mode, 1, 0, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Where the custom aspect ratio is applied (0=game only, 1=full, 2=game no hud, 3=stretch)")
MACRO_CONFIG_INT(TcCustomAspectRatio, tc_custom_aspect_ratio, 0, 0, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Aspect ratio value x100 (0=off, presets: 125=5:4, 133=4:3, 150=3:2, custom: 100-1000)")
MACRO_CONFIG_INT(TcCustomAspectRatioNum, tc_custom_aspect_ratio_num, 16, 0, 100000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom aspect ratio numerator (width), 0=unset")
MACRO_CONFIG_INT(TcCustomAspectRatioDen, tc_custom_aspect_ratio_den, 9, 0, 100000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom aspect ratio denominator (height), 0=unset")
MACRO_CONFIG_STR(TcCustomAspectRatioNumStr, tc_custom_aspect_ratio_num_str, 16, "16", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom aspect ratio numerator as string")
MACRO_CONFIG_STR(TcCustomAspectRatioDenStr, tc_custom_aspect_ratio_den_str, 16, "9", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom aspect ratio denominator as string")

// Indicator Variables
MACRO_CONFIG_COL(TcIndicatorAlive, tc_indicator_alive, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of alive tees in player indicator")
MACRO_CONFIG_COL(TcIndicatorFreeze, tc_indicator_freeze, 65407, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of frozen tees in player indicator")
MACRO_CONFIG_COL(TcIndicatorSaved, tc_indicator_dead, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of tees who is getting saved in player indicator")
MACRO_CONFIG_INT(TcIndicatorOffset, tc_indicator_offset, 42, 16, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(16-128) Offset of indicator position")
MACRO_CONFIG_INT(TcIndicatorOffsetMax, tc_indicator_offset_max, 100, 16, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(16-128) Max indicator offset for variable offset setting")
MACRO_CONFIG_INT(TcIndicatorVariableDistance, tc_indicator_variable_distance, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Indicator circles will be further away the further the tee is")
MACRO_CONFIG_INT(TcIndicatorMaxDistance, tc_indicator_variable_max_distance, 1000, 500, 7000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Maximum tee distance for variable offset")
MACRO_CONFIG_INT(TcIndicatorRadius, tc_indicator_radius, 4, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(1-16) indicator circle size")
MACRO_CONFIG_INT(TcIndicatorOpacity, tc_indicator_opacity, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Opacity of indicator circles")
MACRO_CONFIG_INT(TcPlayerIndicator, tc_player_indicator, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show radial indicator of other tees")
MACRO_CONFIG_INT(TcPlayerIndicatorFreeze, tc_player_indicator_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only show frozen tees in indicator")
MACRO_CONFIG_INT(TcIndicatorTeamOnly, tc_indicator_inteam, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only show indicator while in team")
MACRO_CONFIG_INT(TcIndicatorTees, tc_indicator_tees, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show tees instead of circles")
MACRO_CONFIG_INT(TcIndicatorHideVisible, tc_indicator_hide_visible_tees, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Don't show tees that are on your screen")

MACRO_CONFIG_INT(TcSmoothChatTyping, tc_smooth_chat_typing, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Animate typed chat characters (fall in, bounce out, fade in)")
MACRO_CONFIG_INT(TcSmoothChatSend, tc_smooth_chat_send, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Animate sending chat messages")
MACRO_CONFIG_INT(TcSmoothChatCaret, tc_smooth_chat_caret, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Animate the chat input caret")
MACRO_CONFIG_INT(TcBetterChatNoInputGap, tc_better_chat_no_input_gap, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Move chat messages down into the unused chat input area while the chat is closed")

MACRO_CONFIG_INT(TcSmoothRespawnCamera, tc_smooth_respawn_camera, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Fly the camera smoothly to your tee after respawning")
MACRO_CONFIG_INT(TcSmoothCameraTeleport, tc_smooth_camera_teleport, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Fly the camera smoothly after teleports (command and tile)")
MACRO_CONFIG_INT(TcSmoothCameraSpec, tc_smooth_camera_spec, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Fly the camera smoothly when spectating or pausing")

MACRO_CONFIG_INT(TcScreenshotOnFinish, tc_screenshot_on_finish, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Take a screenshot when you finish a map")

MACRO_CONFIG_INT(TcNotifyWhenAbovePosPlayer, tc_notify_when_above_pos_player, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Appear text that u above player")
MACRO_CONFIG_INT(TcAboveBelowSkew, tc_above_below_skew, 0, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Allowed horizontal offset for the above/below indicator in 1/100 tile (same unit as the Movement Info HUD; 0 = exact same column)")
MACRO_CONFIG_INT(TcNotifyWhenSamePosPlayer, tc_notify_when_same_pos_player, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Appear text that u same pos player")
MACRO_CONFIG_INT(TcNotifyWhenBelowPosPlayer, tc_notify_when_below_pos_player, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Appear text that u below player")
MACRO_CONFIG_INT(TcNotifyWhenPosPlayerPosX, tc_notify_when_pos_player_pos_x, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Appear text when u above/same/below pos player")
MACRO_CONFIG_INT(TcNotifyWhenPosPlayerPosY, tc_notify_when_pos_player_pos_y, 75, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Appear text when u above/same/below pos player")

MACRO_CONFIG_INT(TcShowhudPlayerCheckpoint, tc_showhud_player_checkpoint, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show checkpoint in hud")
MACRO_CONFIG_INT(TcShowhudSmallerHud, tc_showhud_smaller_hud, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "checkpoint and hud will be oneline")
MACRO_CONFIG_INT(TcShowhudVotingPercent, tc_showhud_voting_percent, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show percent in hud")
MACRO_CONFIG_INT(TcSizeOfHeart, tc_size_of_heart, 100, 0, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Heart size")

MACRO_CONFIG_INT(TcNameplateGradient, tc_nameplate_gradient, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Gradient player nicknames")
MACRO_CONFIG_INT(TcNameplateGradientClan, tc_nameplate_gradient_clan, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Gradient player clan tags")
MACRO_CONFIG_INT(TcNameplateGradientEverything, tc_nameplate_gradient_everything, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply gradient to all rendered text")
MACRO_CONFIG_INT(TcNameplateGradientAnimateSpeed, tc_nameplate_gradient_animate_speed, 35, 1, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Speed of the animated gradient sweep, in percent")
MACRO_CONFIG_INT(TcGradientChat, tc_gradient_chat, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Gradient text in chat")
MACRO_CONFIG_INT(TcGradientChatNicknames, tc_gradient_chat_nicknames, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Gradient chat nicknames with the player skin colors")
MACRO_CONFIG_INT(TcGradientChatMessages, tc_gradient_chat_messages, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Gradient chat messages with the message colors from the appearance settings")

MACRO_CONFIG_INT(TcMotionBlur, tc_motion_blur, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Motion blur behind fast tees")
MACRO_CONFIG_INT(TcMotionBlurStrength, tc_motion_blur_strength, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Length of the tee motion blur in percent")
MACRO_CONFIG_INT(TcMotionBlurSelf, tc_motion_blur_self, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Motion blur on your tee")
MACRO_CONFIG_INT(TcMotionBlurDummy, tc_motion_blur_dummy, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Motion blur on your dummy")
MACRO_CONFIG_INT(TcMotionBlurOthers, tc_motion_blur_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Motion blur on other players")
MACRO_CONFIG_INT(TcCursorMotionBlur, tc_cursor_motion_blur, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Motion blur behind the fast cursor")
MACRO_CONFIG_INT(TcCursorMotionBlurStrength, tc_cursor_motion_blur_strength, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Length of the cursor motion blur in percent")
MACRO_CONFIG_INT(TcHudEditorAnimations, tc_hud_editor_animations, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Smooth animations in the HUD editor")

MACRO_CONFIG_INT(TcEnableSwapTimer, tc_enable_swap_timer, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable swap timer")
MACRO_CONFIG_INT(TcEnableSwapTimerOnLeftSide, tc_enable_swap_timer_on_left_side, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Swap timer on left side")
MACRO_CONFIG_INT(TcEnableSwapTimerPosY, tc_enable_swap_timer_pos_y, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Swap timer y")

// Bind Wheel
MACRO_CONFIG_INT(TcResetBindWheelMouse, tc_reset_bindwheel_mouse, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Reset position of mouse when opening bindwheel")

// Regex chat matching
MACRO_CONFIG_STR(TcRegexChatIgnore, tc_regex_chat_ignore, 512, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Filters out chat messages based on a regular expression.")

// Misc visual
MACRO_CONFIG_INT(TcWhiteFeet, tc_white_feet, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render all feet as perfectly white base color")
MACRO_CONFIG_STR(TcWhiteFeetSkin, tc_white_feet_skin, 255, "x_ninja", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Base skin for white feet")
MACRO_CONFIG_INT(TcRenderWeaponsAsGun, tc_render_weapons_as_gun, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Renders weapons as the gun sprite instead of the weapon, with the exception of hammer and ninja (1 = with hue, 2 = without hue)")
MACRO_CONFIG_INT(TcMovingTilesEntities, tc_moving_tiles_entities, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show moving tiles in entities")

MACRO_CONFIG_INT(TcMiniDebug, tc_mini_debug, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show position and angle")

MACRO_CONFIG_INT(TcNotifyWhenLast, tc_last_notify, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Notify when you are last")
MACRO_CONFIG_STR(TcNotifyWhenLastText, tc_last_notify_text, 64, "Last!", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Text for last notify")
MACRO_CONFIG_COL(TcNotifyWhenLastColor, tc_last_notify_color, 256, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color for last notify")
MACRO_CONFIG_INT(TcNotifyWhenLastX, tc_last_notify_x, 20, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Horizontal position for last notify as percentage of screen width")
MACRO_CONFIG_INT(TcNotifyWhenLastY, tc_last_notify_y, 1, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Vertical position for last notify as percentage of screen height")
MACRO_CONFIG_INT(TcNotifyWhenLastSize, tc_last_notify_size, 10, 0, 50, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Font size for last notify")

MACRO_CONFIG_INT(TcRenderCursorSpec, tc_cursor_in_spec, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render your gun cursor when spectating in freeview")
MACRO_CONFIG_INT(TcRenderCursorSpecAlpha, tc_cursor_in_spec_alpha, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Alpha of cursor in freeview")

// MACRO_CONFIG_INT(TcRenderNameplateSpec, tc_render_nameplate_spec, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render nameplates when spectating")

MACRO_CONFIG_INT(TcTinyTees, tc_tiny_tees, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render tees smaller")
MACRO_CONFIG_INT(TcTinyTeeSize, tc_indicator_tees_size, 100, 85, 115, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Define the Size of the Tiny Tee")
MACRO_CONFIG_INT(TcTinyTeesOthers, tc_tiny_tees_others, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render other tees smaller")

MACRO_CONFIG_INT(TcCursorScale, tc_cursor_scale, 100, 0, 500, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Percentage to scale the in game cursor by as a percentage (50 = half, 200 = double)")

// Profiles
MACRO_CONFIG_INT(TcProfileSkin, tc_profile_skin, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply skin in profiles")
MACRO_CONFIG_INT(TcProfileName, tc_profile_name, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply name in profiles")
MACRO_CONFIG_INT(TcProfileClan, tc_profile_clan, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply clan in profiles")
MACRO_CONFIG_INT(TcProfileFlag, tc_profile_flag, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply flag in profiles")
MACRO_CONFIG_INT(TcProfileColors, tc_profile_colors, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply colors in profiles")
MACRO_CONFIG_INT(TcProfileEmote, tc_profile_emote, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply emote in profiles")
MACRO_CONFIG_INT(TcProfileOverwriteClanWithEmpty, tc_profile_overwrite_clan_with_empty, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Overwrite clan name even if profile has an empty clan name")

// Rainbow
MACRO_CONFIG_INT(TcRainbowTees, tc_rainbow_tees, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turn on rainbow client side")
MACRO_CONFIG_INT(TcRainbowHook, tc_rainbow_hook, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Rainbow hook")
MACRO_CONFIG_INT(TcRainbowWeapon, tc_rainbow_weapon, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Rainbow Weapons")

MACRO_CONFIG_INT(TcRainbowOthers, tc_rainbow_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turn on rainbow client side for others")
MACRO_CONFIG_INT(TcRainbowMode, tc_rainbow_mode, 1, 1, 4, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Rainbow mode (1: rainbow, 2: pulse, 3: darkness, 4: random)")
MACRO_CONFIG_INT(TcRainbowSpeed, tc_rainbow_speed, 100, 0, 10000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Rainbow speed as a percentage (50 = half speed, 200 = double speed)")

// War List
MACRO_CONFIG_INT(TcWarList, tc_warlist, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles war list visuals")
MACRO_CONFIG_INT(TcWarListShowClan, tc_warlist_show_clan_if_war, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show clan in nameplate if there is a war")
MACRO_CONFIG_INT(TcWarListReason, tc_warlist_reason, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show war reason")
MACRO_CONFIG_INT(TcWarListChat, tc_warlist_chat, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show war colors in chat")
MACRO_CONFIG_INT(TcWarListScoreboard, tc_warlist_scoreboard, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show war colors in scoreboard")
MACRO_CONFIG_INT(TcWarListAllowDuplicates, tc_warlist_allow_duplicates, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Allow duplicate war entries")
MACRO_CONFIG_INT(TcWarListSpectate, tc_warlist_spectate, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show war colors in spectator menu")

MACRO_CONFIG_INT(TcWarListIndicator, tc_warlist_indicator, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use warlist for indicator")
MACRO_CONFIG_INT(TcWarListIndicatorColors, tc_warlist_indicator_colors, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show warlist colors instead of freeze colors")
MACRO_CONFIG_INT(TcWarListIndicatorAll, tc_warlist_indicator_all, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show all groups")
MACRO_CONFIG_INT(TcWarListIndicatorEnemy, tc_warlist_indicator_enemy, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show players from the first group")
MACRO_CONFIG_INT(TcWarListIndicatorTeam, tc_warlist_indicator_team, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show players from second group")

// Status Bar
MACRO_CONFIG_INT(TcStatusBar, tc_statusbar, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable status bar")

MACRO_CONFIG_INT(TcStatusBar12HourClock, tc_statusbar_12_hour_clock, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use 12 hour clock in local time")
MACRO_CONFIG_INT(TcStatusBarLocalTimeSeconds, tc_statusbar_local_time_seconds, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show seconds in local time")
MACRO_CONFIG_INT(TcStatusBarHeight, tc_statusbar_height, 8, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Height of the status bar")

MACRO_CONFIG_COL(TcStatusBarColor, tc_statusbar_color, 3221225472, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Status bar background color")
MACRO_CONFIG_COL(TcStatusBarTextColor, tc_statusbar_text_color, 4278190335, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Status bar text color")
MACRO_CONFIG_INT(TcStatusBarAlpha, tc_statusbar_alpha, 75, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Status bar background alpha")
MACRO_CONFIG_INT(TcStatusBarTextAlpha, tc_statusbar_text_alpha, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Status bar text alpha")

MACRO_CONFIG_INT(TcStatusBarLabels, tc_statusbar_labels, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show labels on status bar entries")
MACRO_CONFIG_STR(TcStatusBarScheme, tc_statusbar_scheme, 128, "ac pf r", CFGFLAG_CLIENT | CFGFLAG_SAVE, "The order in which to show status bar items")

// Trails
MACRO_CONFIG_INT(TcTeeTrail, tc_tee_trail, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable Tee trails")
MACRO_CONFIG_INT(TcTeeTrailOthers, tc_tee_trail_others, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show tee trails for other players")
MACRO_CONFIG_INT(TcTeeTrailWidth, tc_tee_trail_width, 15, 0, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tee trail width")
MACRO_CONFIG_INT(TcTeeTrailLength, tc_tee_trail_length, 25, 5, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tee trail length")
MACRO_CONFIG_INT(TcTeeTrailAlpha, tc_tee_trail_alpha, 80, 1, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tee trail alpha")
MACRO_CONFIG_COL(TcTeeTrailColor, tc_tee_trail_color, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tee trail color")
MACRO_CONFIG_INT(TcTeeTrailTaper, tc_tee_trail_taper, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Taper tee trail over length")
MACRO_CONFIG_INT(TcTeeTrailFade, tc_tee_trail_fade, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Fade trail alpha over length")
MACRO_CONFIG_INT(TcTeeTrailColorMode, tc_tee_trail_color_mode, 1, 1, 5, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tee trail color mode (1: Solid color, 2: Current Tee color, 3: Rainbow, 4: Color based on Tee speed, 5: Random)")

// Chat Reply
MACRO_CONFIG_INT(TcAutoReplyMuted, tc_auto_reply_muted, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Auto reply to muted players with a message")
MACRO_CONFIG_STR(TcAutoReplyMutedMessage, tc_auto_reply_muted_message, 128, "I have muted you", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Message to reply to muted players")
MACRO_CONFIG_INT(TcAutoReplyMinimized, tc_auto_reply_minimized, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Auto reply when your game is minimized")
MACRO_CONFIG_STR(TcAutoReplyMinimizedMessage, tc_auto_reply_minimized_message, 128, "I am not tabbed in", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Message to reply when your game is minimized")

// Voting
MACRO_CONFIG_INT(TcAutoVoteWhenFar, tc_auto_vote_when_far, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Auto vote no if you far on a map")
MACRO_CONFIG_STR(TcAutoVoteWhenFarMessage, tc_auto_vote_when_far_message, 128, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Message to send when auto far vote happens, leave empty to disable")
MACRO_CONFIG_INT(TcAutoVoteWhenFarTime, tc_auto_vote_when_far_time, 5, 0, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How long until auto vote far happens")

// Font
MACRO_CONFIG_STR(TcCustomFont, tc_custom_font, 255, "DejaVu Sans", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom font face")

// Bg Draw
MACRO_CONFIG_INT(TcBgDrawWidth, tc_bg_draw_width, 5, 1, 50, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Width of background draw strokes")
MACRO_CONFIG_INT(TcBgDrawFadeTime, tc_bg_draw_fade_time, 0, 0, 600, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Time until strokes disappear (0 = never)")
MACRO_CONFIG_INT(TcBgDrawMaxItems, tc_bg_draw_max_items, 128, 0, 2048, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Maximum number of strokes")
MACRO_CONFIG_COL(TcBgDrawColor, tc_bg_draw_color, 14024576, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of background draw strokes")
MACRO_CONFIG_INT(TcBgDrawAutoSaveLoad, tc_bg_draw_auto_save_load, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Automatically save and load background drawings")
MACRO_CONFIG_INT(TcBgDrawClientTimeCps, tc_bg_draw_client_timecps, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use background draw strokes as client-side time checkpoints")

MACRO_CONFIG_INT(TcPracticeTagEnabled, tc_practice_tag_enabled, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable practice command tags")
MACRO_CONFIG_INT(TcPracticeTagShape, tc_practice_tag_shape, 0, 0, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shape of practice command tags (0 = circle, 1 = square, 2 = triangle, 3 = cross)")
MACRO_CONFIG_COL(TcPracticeTagColor, tc_practice_tag_color, 14024576, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of practice command tags")
MACRO_CONFIG_INT(TcPracticeTagSize, tc_practice_tag_size, 20, 4, 64, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size of practice command tags")
MACRO_CONFIG_INT(TcPracticeTagMaxItems, tc_practice_tag_max_items, 128, 0, 2048, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Maximum number of practice command tags per map")

// Translate
MACRO_CONFIG_STR(TcTranslateBackend, tc_translate_backend, 32, "google", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Translate backend to use (google, libretranslate)")
MACRO_CONFIG_STR(TcTranslateTarget, tc_translate_target, 16, "en", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Translate target language (must be 2 character ISO 639 code)")
MACRO_CONFIG_STR(TcTranslateEndpoint, tc_translate_endpoint, 256, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "For backends which need it, endpoint to use (must be https)")
MACRO_CONFIG_STR(TcTranslateKey, tc_translate_key, 256, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "For backends which need it, api key to use")
MACRO_CONFIG_INT(TcTranslateAuto, tc_translate_auto, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Automatically translate incoming messages")
MACRO_CONFIG_INT(TcTranslateOutgoing, tc_translate_outgoing, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Translate your own outgoing chat messages into another language before sending them")
MACRO_CONFIG_STR(TcTranslateOutgoingTarget, tc_translate_outgoing_target, 16, "en", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Language to translate your outgoing messages into (must be 2 character ISO 639 code)")

// Animations
MACRO_CONFIG_INT(TcAnimateWheelTime, tc_animate_wheel_time, 80, 0, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Duration of emote and bind wheel animations, in milliseconds (0 == no animation, 1000 = 1 second)")

// Pets
MACRO_CONFIG_INT(TcPetShow, tc_pet_show, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show a pet")
MACRO_CONFIG_STR(TcPetSkin, tc_pet_skin, 24, "twinbop", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Pet skin")
MACRO_CONFIG_INT(TcPetSize, tc_pet_size, 60, 10, 500, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size of the pet as a percentage of a normal player")
MACRO_CONFIG_INT(TcPetAlpha, tc_pet_alpha, 90, 10, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Alpha of pet (100 = fully opaque, 50 = half transparent)")

MACRO_CONFIG_INT(TcChangeNameNearFinish, tc_change_name_near_finish, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Attempt to change your name when near finish")
MACRO_CONFIG_INT(TcUnlockTeamNearFinish, tc_unlock_team_near_finish, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Send /unlock when near the finish")
MACRO_CONFIG_INT(TcUnlockTeamNearFinishDelay, tc_unlock_team_near_finish_delay, 1000, 0, 10000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Delay before /unlock is sent near the finish in milliseconds")
MACRO_CONFIG_STR(TcFinishName, tc_finish_name, 16, "nameless tee", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Name to change to when near finish when tc_change_name_near_finish is 1")
MACRO_CONFIG_INT(TcSendFinishMessage, tc_send_finish_message, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Send a chat message once when you finish a race")
MACRO_CONFIG_STR(TcFinishMessage, tc_finish_message, 128, "gg", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Message to send when you finish a race")
MACRO_CONFIG_INT(TcFinishMessageDelay, tc_finish_message_delay, 1000, 0, 10000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Delay before the finish message is sent in milliseconds")

MACRO_CONFIG_INT(TcVignette, tc_vignette, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable the custom vignette")
MACRO_CONFIG_INT(TcVignetteMode, tc_vignette_mode, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Vignette position (0 = all edges, 1 = horizontal, 2 = vertical)")
MACRO_CONFIG_INT(TcVignetteRadius, tc_vignette_radius, 50, 1, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Vignette radius in percent of the screen")
MACRO_CONFIG_INT(TcVignetteStrength, tc_vignette_strength, 60, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Vignette strength")
MACRO_CONFIG_INT(TcVignetteSmoothness, tc_vignette_smoothness, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Vignette smoothness")
MACRO_CONFIG_INT(TcVignetteDarken, tc_vignette_darken, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How much the vignette zones are darkened")
MACRO_CONFIG_INT(TcVignetteBlur, tc_vignette_blur, 0, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How much the vignette zones are blurred (Vulkan only)")
MACRO_CONFIG_COL(TcVignetteColor, tc_vignette_color, 0x000000FF, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Vignette zone colour")

// Flags
MACRO_CONFIG_INT(TcTClientSettingsTabs, tc_tclient_settings_tabs, 0, 0, 65536, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Bit flags to disable settings tabs")

// Volleyball
MACRO_CONFIG_INT(TcVolleyBallBetterBall, tc_volleyball_better_ball, 1, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Make frozen players in volleyball look more like volleyballs (0 = disabled, 1 = in volleyball maps, 2 = always)")
MACRO_CONFIG_STR(TcVolleyBallBetterBallSkin, tc_volleyball_better_ball_skin, 24, "Volleyball", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Player skin to use for better volleyball ball")

// Mod
MACRO_CONFIG_INT(TcShowPlayerHitBoxes, tc_show_player_hit_boxes, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show player hit boxes (1 = predicted, 2 = predicted and unpredicted)")
MACRO_CONFIG_INT(TcHideChatBubbles, tc_hide_chat_bubbles, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hide your own chat bubbles, only works when authed in remote console")
MACRO_CONFIG_INT(TcModWeapon, tc_mod_weapon, 0, 0, 1, CFGFLAG_CLIENT, "Run a command (default kill) when you point and shoot at someone, only works when authed in remote console")
MACRO_CONFIG_STR(TcModWeaponCommand, tc_mod_weapon_command, 256, "rcon kill_pl", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Command to run with tc_mod_weapon, id is appended to end of command")

// Run on join
MACRO_CONFIG_STR(TcExecuteOnConnect, tc_execute_on_connect, 100, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_STR(TcExecuteOnJoin, tc_execute_on_join, 100, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(TcExecuteOnJoinDelay, tc_execute_on_join_delay, 7, 7, 50000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tick delay before executing tc_execute_on_join")

// Custom Communities
MACRO_CONFIG_STR(TcCustomCommunitiesUrl, tc_custom_communities_url, 256, "https://raw.githubusercontent.com/TaterClient/ddnet-custom-communities/refs/heads/main/custom-communities-ddnet-info.json", CFGFLAG_CLIENT | CFGFLAG_SAVE, "URL to fetch custom communities from (must be https), empty to disable")

// Discord RPC
MACRO_CONFIG_INT(TcDiscordRPC, tc_discord_rpc, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show Fusion Client in your Discord profile")

MACRO_CONFIG_INT(TcShowLocalTimeSeconds, tc_show_local_time_seconds, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show local time in seconds")

// Configs tab UI
MACRO_CONFIG_INT(TcUiShowDDNet, tc_ui_show_ddnet, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show DDNet domain in Configs tab")
MACRO_CONFIG_INT(TcUiShowTClient, tc_ui_show_tclient, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show TClient domain in Configs tab")
MACRO_CONFIG_INT(TcUiOnlyModified, tc_ui_only_modified, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show only modified settings in Configs tab")
MACRO_CONFIG_INT(TcUiCompactList, tc_ui_compact_list, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use compact row layout in Configs tab")

// Dummy Info
MACRO_CONFIG_INT(TcShowhudDummyPosition, tc_showhud_dummy_position, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ingame HUD (Dummy Position)")
MACRO_CONFIG_INT(TcShowhudDummySpeed, tc_showhud_dummy_speed, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ingame HUD (Dummy Speed)")
MACRO_CONFIG_INT(TcShowhudDummyAngle, tc_showhud_dummy_angle, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ingame HUD (Dummy Aim Angle)")


MACRO_CONFIG_INT(TcDynamicCamera, tc_dynamic_camera, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dynamic camera: zoom depends on tee speed")
MACRO_CONFIG_INT(TcDynamicCameraMode, tc_dynamic_camera_mode, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dynamic camera mode (0 = zoom out with speed, 1 = zoom in with speed)")
MACRO_CONFIG_INT(TcDynamicCameraStrength, tc_dynamic_camera_strength, 25, 0, 150, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dynamic camera strength at full effect (%)")
MACRO_CONFIG_INT(TcDynamicCameraSpeedMin, tc_dynamic_camera_speed_min, 200, 0, 4000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dynamic camera: speed where the effect starts (units/s)")
MACRO_CONFIG_INT(TcDynamicCameraSpeedMax, tc_dynamic_camera_speed_max, 1200, 10, 4000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dynamic camera: speed where the effect is at full strength (units/s)")
MACRO_CONFIG_INT(TcDynamicCameraCurve, tc_dynamic_camera_curve, 1, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dynamic camera response curve (0 = linear, 1 = smooth, 2 = exponential)")
MACRO_CONFIG_INT(TcDynamicCameraSmooth, tc_dynamic_camera_smooth, 40, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dynamic camera transition amount (0 = instant, 100 = slow and smooth)")
MACRO_CONFIG_INT(TcDynamicCameraLimit, tc_dynamic_camera_limit, 0, 0, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dynamic camera zoom out limit (%, 0 = unlimited)")
MACRO_CONFIG_INT(TcDynamicCameraOthers, tc_dynamic_camera_others, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply dynamic camera while spectating other tees")
MACRO_CONFIG_INT(TcDynamicCameraFreeze, tc_dynamic_camera_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply dynamic camera while frozen")
MACRO_CONFIG_INT(TcDynamicCameraHook, tc_dynamic_camera_hook, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply dynamic camera while hooking")

MACRO_CONFIG_INT(TcDriftCamera, tc_drift_camera, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Drift camera: camera leads or lags with speed")
MACRO_CONFIG_INT(TcDriftCameraMode, tc_drift_camera_mode, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Drift camera direction (0 = lead ahead of movement, 1 = lag behind movement)")
MACRO_CONFIG_INT(TcDriftCameraAxis, tc_drift_camera_axis, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Drift camera axis (0 = movement vector, 1 = horizontal only, 2 = vertical only)")
MACRO_CONFIG_INT(TcDriftCameraStrength, tc_drift_camera_strength, 100, 0, 400, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Drift camera strength (%)")
MACRO_CONFIG_INT(TcDriftCameraSpeedRef, tc_drift_camera_speed_ref, 700, 10, 4000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Drift camera: speed where the offset reaches its maximum (units/s)")
MACRO_CONFIG_INT(TcDriftCameraMaxOffset, tc_drift_camera_max_offset, 200, 0, 600, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Drift camera maximum offset (units)")
MACRO_CONFIG_INT(TcDriftCameraSmooth, tc_drift_camera_smooth, 60, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Drift camera transition amount (0 = instant, 100 = slow and smooth)")
MACRO_CONFIG_INT(TcDriftCameraOthers, tc_drift_camera_others, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply drift camera while spectating other tees")
MACRO_CONFIG_INT(TcDriftCameraFreeze, tc_drift_camera_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply drift camera while frozen")
MACRO_CONFIG_INT(TcDriftCameraHook, tc_drift_camera_hook, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply drift camera while hooking")
MACRO_CONFIG_INT(TcCameraTilt, tc_camera_tilt, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Camera tilt: the view rolls left and right with your speed")
MACRO_CONFIG_INT(TcCameraTiltMode, tc_camera_tilt_mode, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Camera tilt axis (0 = horizontal speed, 1 = movement direction, 2 = movement direction inverted)")
MACRO_CONFIG_INT(TcCameraTiltStrength, tc_camera_tilt_strength, 100, 0, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Camera tilt strength (% of the maximum angle)")
MACRO_CONFIG_INT(TcCameraTiltMaxAngle, tc_camera_tilt_max_angle, 50, 0, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Camera tilt maximum angle (0.1 degrees)")
MACRO_CONFIG_INT(TcCameraTiltSpeedRef, tc_camera_tilt_speed_ref, 900, 10, 4000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Camera tilt: speed where the maximum angle is reached (units/s)")
MACRO_CONFIG_INT(TcCameraTiltCurve, tc_camera_tilt_curve, 1, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Camera tilt response curve (0 = linear, 1 = smooth, 2 = exponential)")
MACRO_CONFIG_INT(TcCameraTiltSmooth, tc_camera_tilt_smooth, 55, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Camera tilt transition amount (0 = instant, 100 = slow and smooth)")
MACRO_CONFIG_INT(TcCameraTiltFit, tc_camera_tilt_fit, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Zoom out slightly so the tilted view still covers the whole screen")
MACRO_CONFIG_INT(TcCameraTiltOthers, tc_camera_tilt_others, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply camera tilt while spectating other tees")
MACRO_CONFIG_INT(TcCameraTiltFreeze, tc_camera_tilt_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply camera tilt while frozen")
MACRO_CONFIG_INT(TcCameraTiltHook, tc_camera_tilt_hook, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply camera tilt while hooking")

MACRO_CONFIG_INT(TcJellyTee, tc_jelly_tee, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable jelly tee deformation")
MACRO_CONFIG_INT(TcJellyTeeOthers, tc_jelly_tee_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply jelly tee deformation to other players")
MACRO_CONFIG_INT(TcJellyTeeStrength, tc_jelly_tee_strength, 500, 0, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Strength of jelly tee deformation")
MACRO_CONFIG_INT(TcJellyTeeDuration, tc_jelly_tee_duration, 30, 1, 500, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Duration of jelly tee deformation")

MACRO_CONFIG_INT(TcTeeBreathing, tc_tee_breathing, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Idle breathing animation on your tee")
MACRO_CONFIG_INT(TcTeeBreathingOthers, tc_tee_breathing_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply the idle breathing animation to other players")

MACRO_CONFIG_INT(TcShowHitBox, tc_show_hitbox, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show a dot at the center of your real hitbox")
MACRO_CONFIG_COL(TcShowHitBoxColor, tc_show_hitbox_color, 0xFFFFFFFFU, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of the hitbox center dot")
MACRO_CONFIG_INT(TcShowHitBoxOthers, tc_show_hitbox_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show the hitbox dot on other players")

MACRO_CONFIG_INT(TcKeystrokes, tc_keystrokes, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show keystrokes HUD")
MACRO_CONFIG_INT(TcKeystrokesOpacity, tc_keystrokes_pressed_opacity, 92, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Opacity of pressed keys in keystrokes (%)")
MACRO_CONFIG_INT(TcKeystrokesCps, tc_keystrokes_cps, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show current CPS (LMB and RMB) in keystrokes")

MACRO_CONFIG_INT(TcFinishPrediction, tc_finish_prediction, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show finish prediction")
MACRO_CONFIG_INT(TcFinishPredictionTimeMode, tc_finish_prediction_time_mode, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Finish prediction time mode (0 = remaining, 1 = predicted total)")
MACRO_CONFIG_INT(TcFinishPredictionShowTime, tc_finish_prediction_show_time, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show the predicted time")
MACRO_CONFIG_INT(TcFinishPredictionShowMillis, tc_finish_prediction_show_millis, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show milliseconds in the predicted time")
MACRO_CONFIG_INT(TcFinishPredictionShowPercent, tc_finish_prediction_show_percent, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show the predicted progress percentage")
MACRO_CONFIG_INT(TcFinishPredictionShowAlways, tc_finish_prediction_show_always, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show finish prediction before the race starts")
MACRO_CONFIG_INT(TcFinishPredictionSpeedMode, tc_finish_prediction_speed_mode, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Prediction speed source (0 = current speed, 1 = measured average)")
MACRO_CONFIG_INT(TcFinishPredictionSmooth, tc_finish_prediction_smooth, 60, 0, 95, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Smoothing of the predicted time (0 = instant, 95 = slow)")

MACRO_CONFIG_INT(TcMuteOthersSounds, tc_mute_others_sounds, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Mute every sound that comes from other players")
MACRO_CONFIG_INT(TcInputs, tc_inputs, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Input prediction mode (0 = off, 1 = fast, 2 = best)")
MACRO_CONFIG_INT(TcBestInputAmount, tc_best_input_amount, 0, 0, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Best input prediction amount in 0.01 ticks")
MACRO_CONFIG_INT(TcBestInputSmoothing, tc_best_input_smoothing, 0, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Best input smoothing amount (0-100%)")
MACRO_CONFIG_INT(TcBestInputLatencyComp, tc_best_input_latency_comp, 0, 0, 50, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Best input latency compensation (0-50%)")
MACRO_CONFIG_INT(TcBestInputInterpolation, tc_best_input_interpolation, 1, 1, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Best input interpolation mode (1 = linear, 2 = cubic, 3 = smooth)")
MACRO_CONFIG_INT(TcBestInputOthers, tc_best_input_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply best input to other tees")
MACRO_CONFIG_INT(TcSaikoInputAmount, tc_saiko_input_amount, 0, 0, 500, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Saiko input amount in 0.01 ticks")
MACRO_CONFIG_INT(TcSaikoInputOthers, tc_saiko_input_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply Saiko input to other tees")
MACRO_CONFIG_INT(TcDeltaInputAmount, tc_delta_input_amount, 0, 0, 500, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Delta input amount in 0.01 ticks")
MACRO_CONFIG_INT(TcDeltaInputOthers, tc_delta_input_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply delta input to other tees")
MACRO_CONFIG_INT(TcFInputAmount, tc_f_input_amount, 0, 0, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "F input amount in 0.001-tick units (0=off, 1000=1.000 ticks, max 5000=5.000 ticks)")
MACRO_CONFIG_INT(TcFInputOthers, tc_f_input_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply F input to other tees")
MACRO_CONFIG_INT(TcCloudInputOthers, tc_cloud_input_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply cloud input to other tees")
MACRO_CONFIG_INT(TcLaserGlow, tc_laser_glow, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable Laser Glow visual effect")
MACRO_CONFIG_INT(TcLaserGlowPower, tc_laser_glow_power, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Laser Glow power/intensity (0-100)")
MACRO_CONFIG_INT(TcLaserGlowMode, tc_laser_glow_mode, 0, 0, 5, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Laser Glow mode: 0=Classic, 1=Prism, 2=Pulse, 3=Crystal, 4=Plasma, 5=Aurora")
MACRO_CONFIG_COL(TcLaserGlowColor, tc_laser_glow_color, 5635840, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Laser Glow custom color")
MACRO_CONFIG_INT(TcShotgunGlow, tc_shotgun_glow, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable Shotgun Glow visual effect")
MACRO_CONFIG_INT(TcShotgunGlowPower, tc_shotgun_glow_power, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shotgun Glow power/intensity (0-100)")
MACRO_CONFIG_INT(TcShotgunGlowMode, tc_shotgun_glow_mode, 0, 0, 5, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shotgun Glow mode: 0=Classic, 1=Prism, 2=Pulse, 3=Crystal")
MACRO_CONFIG_COL(TcShotgunGlowColor, tc_shotgun_glow_color, 5635840, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shotgun Glow custom color")
MACRO_CONFIG_INT(TcRocketGlow, tc_rocket_glow, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable Rocket/Grenade Glow visual effect")
MACRO_CONFIG_INT(TcRocketGlowPower, tc_rocket_glow_power, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Rocket Glow power/intensity (0-100)")
MACRO_CONFIG_INT(TcRocketGlowMode, tc_rocket_glow_mode, 0, 0, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Rocket Glow mode: 0=Classic, 1=Prism, 2=Pulse, 3=Crystal")
MACRO_CONFIG_COL(TcRocketGlowColor, tc_rocket_glow_color, 2555648, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Rocket Glow custom color")
MACRO_CONFIG_INT(TcMemeNukeExplosion, tc_meme_nuke_explosion, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable meme mega nuclear explosion on rocket impact")
MACRO_CONFIG_INT(TcServerMapPreview, tc_server_map_preview, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show a live map preview under the selected server")
MACRO_CONFIG_INT(TcFreezeLightning, tc_freeze_lightning, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Strike a lightning bolt when a tee gets frozen")
MACRO_CONFIG_COL(TcFreezeLightningColor, tc_freeze_lightning_color, 9371558, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of the freeze lightning bolt")
MACRO_CONFIG_INT(TcFreezeLightningHeight, tc_freeze_lightning_height, 400, 100, 900, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Height the freeze lightning bolt comes down from")
MACRO_CONFIG_INT(TcFreezeLightningThickness, tc_freeze_lightning_thickness, 100, 50, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Thickness of the freeze lightning bolt in percent")
MACRO_CONFIG_INT(TcFreezeLightningBranches, tc_freeze_lightning_branches, 3, 0, 8, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Number of side branches of the freeze lightning bolt")
MACRO_CONFIG_INT(TcFreezeLightningImpact, tc_freeze_lightning_impact, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show impact flash, sparks and frost when the lightning hits")
MACRO_CONFIG_INT(TcFreezeLightningShake, tc_freeze_lightning_shake, 35, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Screen shake when the lightning strikes you, 0 to disable")
MACRO_CONFIG_INT(TcHookStyle, tc_hook_style, 0, 0, 8, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Effect where the hook grips the world (0 = none, 1 = whirlwind, 2 = black hole, 3 = magic, 4 = fire, 5 = troll, 6 = rainbow, 7 = lightning, 8 = tentacle)")
MACRO_CONFIG_INT(TcHookRope, tc_hook_rope, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draw the hook itself in the style of the hook effect")
MACRO_CONFIG_INT(TcHookTether, tc_hook_tether, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "A lasso and pulses along the hook when you hook another player")
MACRO_CONFIG_INT(TcHookLassoBox, tc_hook_lasso_box, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Corner brackets on the player your hook holds")
MACRO_CONFIG_INT(TcAirJump, tc_air_jump, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "A ring of light under your feet when you jump in the air, colored like the hook")
MACRO_CONFIG_INT(TcAirJumpHideParticles, tc_air_jump_hide_particles, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hide the normal air jump particles while the air jump ring is on")
MACRO_CONFIG_INT(TcHammerSlash, tc_hammer_slash, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "A glowing slash when you swing the hammer, colored like the hook")
MACRO_CONFIG_INT(TcHammerHideParticles, tc_hammer_hide_particles, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hide the normal hammer hit particles while the hammer slash is on")
MACRO_CONFIG_INT(TcHookChargeTime, tc_hook_charge_time, 1200, 200, 4000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Milliseconds of holding the hook until its effect is fully grown")
MACRO_CONFIG_INT(TcHookSize, tc_hook_size, 100, 50, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size of the hook effect in percent")
MACRO_CONFIG_COL(TcHookWindColor, tc_hook_wind_color, 8343024, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of the wind lines in the hook whirlwind")
MACRO_CONFIG_INT(TcHookWindBurst, tc_hook_wind_burst, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Burst out a gust of wind when the hook grips")
MACRO_CONFIG_INT(TcHookWindEye, tc_hook_wind_eye, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draw the turning eye in the middle of the hook whirlwind")
MACRO_CONFIG_INT(TcHookWindDensity, tc_hook_wind_density, 100, 25, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Amount of wind lines in the hook whirlwind in percent")
MACRO_CONFIG_INT(TcHookWindSpin, tc_hook_wind_spin, 100, 25, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turning speed of the hook whirlwind in percent")
MACRO_CONFIG_INT(TcHookWindThickness, tc_hook_wind_thickness, 100, 25, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Thickness of the wind lines in the hook whirlwind in percent")
MACRO_CONFIG_INT(TcHookWindOpacity, tc_hook_wind_opacity, 100, 10, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Opacity of the wind in the hook whirlwind in percent")
MACRO_CONFIG_INT(TcHookWindLeafAmount, tc_hook_wind_leaf_amount, 100, 0, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Amount of leaves the hook whirlwind carries in percent, 0 for none")
MACRO_CONFIG_INT(TcHookWindDust, tc_hook_wind_dust, 100, 0, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Amount of dust the hook whirlwind stirs up in percent, 0 for none")
MACRO_CONFIG_COL(TcHookHoleColor, tc_hook_hole_color, 1245078, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of the black hole's accretion disk")
MACRO_CONFIG_INT(TcHookHoleDisk, tc_hook_hole_disk, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draw the black hole's accretion disk and lensed light")
MACRO_CONFIG_INT(TcHookHoleStreaks, tc_hook_hole_streaks, 100, 0, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Amount of matter falling into the black hole in percent, 0 for none")
MACRO_CONFIG_INT(TcHookHoleSpin, tc_hook_hole_spin, 100, 25, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turning speed of the black hole's disk in percent")
MACRO_CONFIG_COL(TcHookMagicColor, tc_hook_magic_color, 12896411, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of the hook's spell circle")
MACRO_CONFIG_INT(TcHookMagicRunes, tc_hook_magic_runes, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draw runes in the hook's spell circle and chain")
MACRO_CONFIG_INT(TcHookMagicSpin, tc_hook_magic_spin, 100, 25, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turning speed of the hook's spell circle in percent")
MACRO_CONFIG_INT(TcHookMagicSparkles, tc_hook_magic_sparkles, 100, 0, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Amount of sparkles given off by the hook magic in percent, 0 for none")
MACRO_CONFIG_INT(TcHookRainbowSpeed, tc_hook_rainbow_speed, 100, 25, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How fast the rainbow hook's colors flow in percent")
MACRO_CONFIG_INT(TcHookRainbowGlitter, tc_hook_rainbow_glitter, 100, 0, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Amount of glitter from the rainbow hook in percent, 0 for none")
MACRO_CONFIG_INT(TcHookRainbowRays, tc_hook_rainbow_rays, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Spinning light rays around the rainbow grip")
MACRO_CONFIG_INT(TcHookRainbowWidth, tc_hook_rainbow_width, 100, 50, 250, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Width of the rainbow hook ribbon in percent")
MACRO_CONFIG_COL(TcHookFireColor, tc_hook_fire_color, 982912, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of the hook's fire")
MACRO_CONFIG_INT(TcHookFireSmoke, tc_hook_fire_smoke, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Smoke rising from the hook's fire")
MACRO_CONFIG_INT(TcHookFireIntensity, tc_hook_fire_intensity, 100, 25, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Amount of flame in the hook's fire in percent")
MACRO_CONFIG_INT(TcHookFireEmbers, tc_hook_fire_embers, 100, 0, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Amount of embers rising from the hook's fire in percent, 0 for none")
MACRO_CONFIG_COL(TcHookBoltColor, tc_hook_bolt_color, 9240520, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of the lightning hook")
MACRO_CONFIG_INT(TcHookBoltJitter, tc_hook_bolt_jitter, 100, 25, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How wildly the lightning hook whips about in percent")
MACRO_CONFIG_INT(TcHookBoltBranches, tc_hook_bolt_branches, 3, 0, 6, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Number of forks splitting off the lightning hook")
MACRO_CONFIG_INT(TcHookBoltSparks, tc_hook_bolt_sparks, 100, 0, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Amount of sparks thrown off where the lightning strikes, 0 for none")
MACRO_CONFIG_INT(TcHookBoltArc, tc_hook_bolt_arc, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Arcs of electricity crawling over the tile the lightning hook holds")
MACRO_CONFIG_COL(TcHookTentacleColor, tc_hook_tentacle_color, 570478, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of the tentacle hook")
MACRO_CONFIG_INT(TcHookTentacleWaves, tc_hook_tentacle_waves, 100, 25, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How strongly the tentacle hook writhes in percent")
MACRO_CONFIG_INT(TcHookTentacleThickness, tc_hook_tentacle_thickness, 100, 50, 250, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Thickness of the tentacle hook in percent")
MACRO_CONFIG_INT(TcHookTentacleSuckers, tc_hook_tentacle_suckers, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draw suckers along the underside of the tentacle hook")
MACRO_CONFIG_INT(TcHookTentacleSlime, tc_hook_tentacle_slime, 100, 0, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Amount of slime dripping off the tentacle hook, 0 for none")
MACRO_CONFIG_COL(TcHookFrostColor, tc_hook_frost_color, 8762336, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of the frost hook")
MACRO_CONFIG_INT(TcHookFrostCrystals, tc_hook_frost_crystals, 9, 3, 24, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Number of ice crystals growing around the frost hook grip")
MACRO_CONFIG_INT(TcHookFrostGrowth, tc_hook_frost_growth, 100, 25, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Length of the ice crystals in percent")
MACRO_CONFIG_INT(TcHookFrostMist, tc_hook_frost_mist, 100, 0, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Amount of cold mist around the frost hook, 0 for none")
MACRO_CONFIG_INT(TcHookFrostSnow, tc_hook_frost_snow, 100, 0, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Amount of snow drifting off the frost hook, 0 for none")
MACRO_CONFIG_INT(TcHookRings, tc_hook_rings, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ring effects at the end of hook styles")
MACRO_CONFIG_INT(TcGatheredEnergy, tc_gathered_energy, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Energy gathers around you while your hook holds another tee")
MACRO_CONFIG_INT(TcGatheredEnergyAmount, tc_gathered_energy_amount, 100, 25, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Amount of energy motes gathering around you in percent")
MACRO_CONFIG_INT(TcGatheredEnergyAura, tc_gathered_energy_aura, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draw the aura and the ground ring of the gathered energy")
MACRO_CONFIG_INT(TcGatheredEnergyBurst, tc_gathered_energy_burst, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Blow the gathered energy apart when the hook comes off the other tee")
MACRO_CONFIG_INT(TcAtmosphere, tc_atmosphere, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Weather around you")
MACRO_CONFIG_INT(TcWeather, tc_weather, 0, 0, 5, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Weather type (0 = snow, 1 = rain, 2 = fireflies, 3 = dust, 4 = leaves, 5 = sakura)")
MACRO_CONFIG_INT(TcWeatherAmount, tc_weather_amount, 100, 25, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Amount of weather in percent")
MACRO_CONFIG_INT(TcWeatherSize, tc_weather_size, 100, 25, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size of the weather in percent")
MACRO_CONFIG_INT(TcWeatherOpacity, tc_weather_opacity, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Opacity of the weather in percent")
MACRO_CONFIG_COL(TcWeatherSnowColor, tc_weather_snow_color, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of the snow")
MACRO_CONFIG_INT(TcWeatherSnowflakes, tc_weather_snowflakes, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Snow look (0 = dots, 1 = snowflakes)")
MACRO_CONFIG_COL(TcWeatherRainColor, tc_weather_rain_color, 10063569, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of the rain")
MACRO_CONFIG_COL(TcWeatherFireflyColor, tc_weather_firefly_color, 3791782, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of the fireflies")
MACRO_CONFIG_COL(TcWeatherLeafColor, tc_weather_leaf_color, 5086827, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of the falling leaves")
MACRO_CONFIG_COL(TcWeatherSakuraColor, tc_weather_sakura_color, 15905753, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of the sakura petals")
MACRO_CONFIG_INT(TcAmbientDust, tc_ambient_dust, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Drift motes of dust through the world for atmosphere")
MACRO_CONFIG_COL(TcAmbientDustColor, tc_ambient_dust_color, 1981660, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of the ambient dust motes")
MACRO_CONFIG_INT(TcAmbientDustAmount, tc_ambient_dust_amount, 100, 25, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Amount of ambient dust motes in percent")
MACRO_CONFIG_INT(TcAmbientDustSize, tc_ambient_dust_size, 100, 25, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size of the ambient dust motes in percent")
MACRO_CONFIG_INT(TcAmbientDustOpacity, tc_ambient_dust_opacity, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Opacity of the ambient dust motes in percent")

MACRO_CONFIG_INT(TcRaycastSounds, tc_raycast_sounds, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Raycast 3D sounds: walls muffle positional sounds and block teammate sounds")
MACRO_CONFIG_INT(TcRaycastSoundsTeammates, tc_raycast_sounds_teammates, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Mute teammate sounds that are blocked by walls")
MACRO_CONFIG_INT(TcRaycastSoundsOcclusion, tc_raycast_sounds_occlusion, 55, 5, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Volume of a sound after it passes one wall, in percent")
MACRO_CONFIG_INT(TcRaycastSoundsEcho, tc_raycast_sounds_echo, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Add echo to sounds in enclosed spaces")
MACRO_CONFIG_INT(TcRaycastSoundsEchoVolume, tc_raycast_sounds_echo_volume, 35, 5, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Volume of the echo, in percent")
MACRO_CONFIG_INT(TcFreezeLightningStyle, tc_freeze_lightning_style, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Freeze effect style (0 = lightning, 1 = frost nova, 2 = ice shards)")
MACRO_CONFIG_INT(TcFreezeLightningSize, tc_freeze_lightning_size, 100, 50, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size of the frost nova and ice shard effects in percent")
MACRO_CONFIG_INT(TcMemeNukeShake, tc_meme_nuke_shake, 100, 0, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Screen shake intensity for nuke rocket explosion (0-200%)")
MACRO_CONFIG_INT(TcMemeNukeScale, tc_meme_nuke_scale, 100, 20, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Scale of the meme nuke explosion (20-300%)")

MACRO_CONFIG_INT(TcLyrics, tc_lyrics, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show the lyrics of the current song as 3D text in the world")
MACRO_CONFIG_STR(TcLyricsFile, tc_lyrics_file, 128, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Read the lyrics from this .lrc file instead of looking them up online")
MACRO_CONFIG_INT(TcLyricsSize, tc_lyrics_size, 42, 12, 120, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Height of the lyrics text in world units")
MACRO_CONFIG_INT(TcLyricsLines, tc_lyrics_lines, 2, 1, 6, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How many lyric lines can be visible at the same time")
MACRO_CONFIG_INT(TcLyricsExtrusion, tc_lyrics_extrusion, 35, 0, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Depth of the 3D extrusion in percent, 0 for flat text")
MACRO_CONFIG_INT(TcLyricsTilt, tc_lyrics_tilt, 30, 0, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How strongly every line tilts in percent")
MACRO_CONFIG_INT(TcLyricsDrift, tc_lyrics_drift, 45, 0, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Drift speed of the lines in percent")
MACRO_CONFIG_INT(TcLyricsGlitter, tc_lyrics_glitter, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Gold glitter and sparkles on the lyrics")
MACRO_CONFIG_COL(TcLyricsColor, tc_lyrics_color, 4280221599, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Gold color of the lyrics text")
MACRO_CONFIG_INT(TcLyricsBeat, tc_lyrics_beat, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Pulse the lyrics with the beat of the music")
MACRO_CONFIG_INT(TcLyricsOffset, tc_lyrics_offset, 0, -20000, 20000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Lyrics timing offset in milliseconds")
MACRO_CONFIG_INT(TcLyricsTest, tc_lyrics_test, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show built-in demo lines instead of looking up the current song")
MACRO_CONFIG_INT(TcLyricsClean, tc_lyrics_clean, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Plain readable text: no extrusion, no glitter, flat color")
