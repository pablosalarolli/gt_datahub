cmake_minimum_required(VERSION 3.23)

project(gt_datahub)

# Inicializa pacote gtfw
find_package(gtfw_cmake REQUIRED)

gtfw_package_init()

##############################################
# INCLUINDO DEPENDÊNCIAS EXTERNAS
##############################################


##############################################
# CRIANDO A LIB
##############################################

set(GTDATAHUB_SRC_FILES
)

set(GTDATAHUB_HEADER_FILES
    include/gt_datahub/visibility_control.hpp
    include/gt_datahub/timestamp.hpp
    include/gt_datahub/data_type.hpp
    include/gt_datahub/quality.hpp
    include/gt_datahub/variable_role.hpp
    include/gt_datahub/value.hpp
    include/gt_datahub/variable_definition.hpp
    include/gt_datahub/variable_state.hpp
    include/gt_datahub/update_request.hpp
    include/gt_datahub/errors.hpp
)

add_library(gt_datahub INTERFACE ${GTDATAHUB_SRC_FILES} ${GTDATAHUB_HEADER_FILES} )

target_include_directories(gt_datahub
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

##############################################
# LINK DE DEPENDÊNCIAS EXTERNAS
##############################################

# # Bibliotecas para link da lib

##############################################
# AJUSTES FINAIS DO PACOTE
##############################################

# Registra o target
gtfw_package_register(TARGETS gt_datahub)

# Enquandra os targets aos moldes do GTFW
gtfw_target_setup(TARGETS gt_datahub)

# gtfw_setup_docs()

gtfw_setup_tests()

gtfw_setup_examples()

# Instala e exporta o pacote
gtfw_package_install()
