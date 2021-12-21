# Global  rules

TEMPLATE = subdirs

SUBDIRS += \
    drottningholm_collection_drt \
    drottningholm_collection_fixed \
    drottningholm_bidirectional_fixed \
    drottningholm_bidirectional_mixed_day2day \
    sf_fixed_day2day \
    sf_fixed \
    controlcenter \
    routestest \
    drttest \
    pentafeeder_fixed \
    pentafeeder_drt \
    pentafeeder_mixed_day2day \
    fixedwithflexible_walking \
    fixedwithflexible_day2day \
    fixedwithflexible \
    drt_algorithms \
    fixedwithflexible_2stop_2link

	
CONFIG += sdk_no_version_check # for mac
