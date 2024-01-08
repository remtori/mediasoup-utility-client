#pragma once

#include "conference_manager.hpp"
#include "viewer_manager.hpp"

void setup_livestream_bot_ui(std::shared_ptr<ViewerManager> manager, size_t default_viewer_count);

void setup_conference_bot_ui(std::shared_ptr<ConferenceManager> manager, size_t room_count, size_t user_count, size_t base_room_id);
