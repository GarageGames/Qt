TEMPLATE = subdirs

qtHaveModule(quick) {
    SUBDIRS += places \
               places_list \
               places_map \
               mapviewer \
               planespotter
}
