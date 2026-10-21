// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// A copy of the save folder (<user>/save, the game's rmcsave: drive) at each
// start, before the game runs: <user>/save-backups/<date>_<time>/, the newest
// five kept. The cheats write to the career profile and a freeze could catch a
// save half written; the Android launcher's Garage > Restore save copies one
// back. A start whose saves equal the newest backup adds none.
#pragma once

namespace save_backup
{
    void BackUp();
}
