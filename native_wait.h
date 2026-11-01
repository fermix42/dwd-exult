#ifndef NATIVE_WAIT_H
#define NATIVE_WAIT_H

class Game_window;

// Selection/preview is read-only. Only advance() crosses the confirmation boundary.
// No UI, test-driver or configuration dependency belongs in this service.
namespace Native_wait {
struct Result {
	int requested_minutes;
	int completed_minutes;
	const char* reason;
	int completed_ticks = 0; // Exact partial duration; 25 calendar ticks per minute.
};
const char* eligibility(Game_window& world);
Result advance(Game_window& world, int minutes);
}
#endif
