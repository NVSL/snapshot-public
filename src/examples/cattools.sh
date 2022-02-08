# @brief Helper functions for managing Intel Cache Allocation Technology
# @author Suyash Mahar

CAT_TOOLS_LOC="${CAT_TOOLS_LOC:-/home/smahar/git/intel-cmt-cat/examples/c/CAT_MBA}"
SUDO=${SUDO:-sudo}
if [ $(id -u) -eq 0 ]; then
    SUDO=
fi

show_masks() {
    "$SUDO" "${CAT_TOOLS_LOC}/allocation_app_l3cat"
    return "$?"
}

show_associations() {
    "$SUDO" "${CAT_TOOLS_LOC}/association_app"
    return "$?"
}

set_mask() {
    COS="$1"
    MASK="$2"

    "$SUDO" "${CAT_TOOLS_LOC}/allocation_app_l3cat" "$COS" "$MASK" > /dev/null

    return "$?"
}

set_association() {
    COS="$1"
    CPU="$2"

    "$SUDO" "${CAT_TOOLS_LOC}/association_app" "$COS" "$CPU" > /dev/null

    return "$?"
}

set_associations() {
    set_association 1 0

    return "$?"
}

reset() {
    "$SUDO" "${CAT_TOOLS_LOC}/reset_app" > /dev/null

    return "$?"
}
