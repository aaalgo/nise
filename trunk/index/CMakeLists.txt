SET(TOOLS import-nutch import merge graph-hash graph-join mapid group group-index sketch-index number cluster import-id cluster2id graph-index download)
FOREACH(TOOL ${TOOLS})
ADD_EXECUTABLE(${TOOL} ${TOOL}.cpp)
TARGET_LINK_LIBRARIES(${TOOL} ${DEFAULT_LIBRARIES})
ENDFOREACH(TOOL)

