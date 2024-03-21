#pragma once
#include "libOTe/version.h"

#define LIBOTE_VERSION (LIBOTE_VERSION_MAJOR * 10000 + LIBOTE_VERSION_MINOR * 100 + LIBOTE_VERSION_PATCH)

// build the library bit poly mul integration
#define ENABLE_BITPOLYMUL ON

// build the library with "simplest" Base OT enabled
/* #undef ENABLE_SIMPLESTOT */

// build the library with the ASM "simplest" Base OT enabled
/* #undef ENABLE_SIMPLESTOT_ASM */

// build the library with POPF Base OT using Ristretto KA enabled
#define ENABLE_MRR ON

// build the library with POPF Base OT using Moeller KA enabled
/* #undef ENABLE_MRR_TWIST */

// build the library with Masney Rindal Base OT enabled
/* #undef ENABLE_MR */

// build the library with Masney Rindal Kyber Base OT enabled
/* #undef ENABLE_MR_KYBER */



// build the library with Keller Orse Scholl OT-Ext enabled
/* #undef ENABLE_KOS */

// build the library with IKNP OT-Ext enabled
#define ENABLE_IKNP ON

// build the library with Silent OT Extension enabled
#define ENABLE_SILENTOT ON

// build the library with SoftSpokenOT enabled
#define ENABLE_SOFTSPOKEN_OT ON



// build the library with KOS Delta-OT-ext enabled
/* #undef ENABLE_DELTA_KOS */

// build the library with OOS 1-oo-N OT-Ext enabled
/* #undef ENABLE_OOS */

// build the library with KKRT 1-oo-N OT-Ext enabled
/* #undef ENABLE_KKRT */

// build the library with silent vole enabled
#define ENABLE_SILENT_VOLE ON

// build the library with no silver security warning
/* #undef NO_SILVER_WARNING */

// build the library with no KOS security warning
/* #undef NO_KOS_WARNING */

#if defined(ENABLE_SIMPLESTOT_ASM) && defined(_MSC_VER)
    #undef ENABLE_SIMPLESTOT_ASM
    #pragma message("ENABLE_SIMPLESTOT_ASM should not be defined on windows.")
#endif
#if defined(ENABLE_MR_KYBER) && defined(_MSC_VER)
    #undef ENABLE_MR_KYBER
    #pragma message("ENABLE_MR_KYBER should not be defined on windows.")
#endif
        
