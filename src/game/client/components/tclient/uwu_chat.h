#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_UWU_CHAT_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_UWU_CHAT_H

// rewrites own outgoing chat so it sounds personal; driven by fc_uwu_chat_*
namespace UwuChat {
// rewritten line, or the original untouched (disabled, team off, or a command)
const char *Transform(int Team, const char *pText, char *pOut, int OutSize);
} // namespace UwuChat

#endif
