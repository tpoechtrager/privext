This patch allows you to hide real player ips from extinfo and to keep tools
like WC/CSL functioning properly at the same time.

Requirements: `libgeoip-dev`.

Please study `src/fpsgame/privext.cpp` to see how it works.
