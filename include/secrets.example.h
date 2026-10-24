#pragma once

// Template for include/secrets.h (gitignored). Copy it and change the values;
// builds fall back to this file when secrets.h is absent.
#define SECRET_AP_PASSWORD "PanelSetup32" // Setup hotspot password, 8-63 characters.
// Optional login (user "admin") for /panel and /api/*. Empty = no login.
#define SECRET_PANEL_PASSWORD ""
