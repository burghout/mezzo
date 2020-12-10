# Global  rules

TEMPLATE = subdirs

SUBDIRS += \
    drottningholm_collection_drt \
    sf_fixed_day2day \
    sf_fixed \
    controlcenter \
    routestest \
    drttest \
    pentafeeder_fixed \
    pentafeeder_drt \
    fixedwithflexible_walking \
    fixedwithflexible_day2day \
    fixedwithflexible \
    drt_algorithms

	
CONFIG += sdk_no_version_check # for mac
