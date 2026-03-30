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
)

add_library(gt_datahub INTERFACE ${GTDATAHUB_SRC_FILES} ${GTDATAHUB_HEADER_FILES} )

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
