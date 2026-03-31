#include "processor.h"
#include "controller.h"
#include "plugincids.h"
#include "version.h"

#include "public.sdk/source/main/pluginfactory.h"

using namespace Steinberg::Vst;

BEGIN_FACTORY_DEF (stringCompanyName, stringCompanyWeb, stringCompanyEmail)

    DEF_CLASS2 (INLINE_UID_FROM_FUID (TooltipTest::ProcessorUID),
                PClassInfo::kManyInstances,
                kVstAudioEffectClass,
                stringFileDescription,
                Vst::kDistributable,
                PluginCategory,
                FULL_VERSION_STR,
                kVstVersionString,
                TooltipTest::Processor::createInstance)

    DEF_CLASS2 (INLINE_UID_FROM_FUID (TooltipTest::ControllerUID),
                PClassInfo::kManyInstances,
                kVstComponentControllerClass,
                stringFileDescription " Controller",
                0,
                "",
                FULL_VERSION_STR,
                kVstVersionString,
                TooltipTest::Controller::createInstance)

END_FACTORY