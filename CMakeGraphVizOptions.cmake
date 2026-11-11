set(GRAPHVIZ_GRAPH_HEADER "graph [\n  fontname = \"Montserrat SemiBold\";\n];\nnode [\n  fontsize = \"12\";\n  fontname = \"Montserrat\";\n];\nedge [\n  fontname = \"Montserrat Light\";\n];")
set(GRAPHVIZ_GENERATE_PER_TARGET FALSE)
set(GRAPHVIZ_GENERATE_DEPENDERS FALSE)
set(GRAPHVIZ_IGNORE_TARGETS
    "CONAN_LIB::.*"
    "GTest::.*"
    ".*_DEPS_TARGET"
    ".*-test"
    "SDL2::.*"
    "SDL2_.*::.*"
    "pybind11::.*"
    "Python3::.*"
    "libwebp:.*"
    "WebP::.*"
    "GLEW::*"
    "JPEG::.*"
    "PNG::.*"
    "ZLIB::.*"
    "TIFF::.*"
    "LibLZMA::.*"
    "jbig::.*"
    "libdeflate::.*"
    "zstd::.*"
    "clipper::.*"
    "draco::.*"
    "minizip::.*"
    "BZip2::.*"
    "openddlparser::.*"
    "poly2tri::.*"
    "pugixml::.*"
    "zip::.*"
    "ryml::.*"
    "c4core::.*"
    "FastFloat::.*"
    "glu::.*"
    "opengl::.*"
    glfw
    )

if (WIN32)
list(APPEND GRAPHVIZ_IGNORE_TARGETS
    ".*32"
    setupapi
    shell
    uuid
    version
    winmm
)
endif()
