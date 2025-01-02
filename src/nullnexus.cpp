#include "config.h"
#include "nullnexus.hpp"

namespace nullnexus {
void init() {}
void shutdown() {}
void update() {}
void sendChat(const std::string &) {}
void sendTeamchat(const std::string &) {}
void sendGift(unsigned, unsigned) {}
void sendKillstreak(unsigned) {}
void sendUserMessage(const std::string &, const std::string &) {}
void processChatMessage(const std::string &, const std::string &) {}
void processKillstreak(const std::string &, int) {}
void processGift(const std::string &, const std::string &, const std::string &) {}
void processUserMessage(const std::string &, const std::string &, const std::string &) {}
}
