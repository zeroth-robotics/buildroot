################################################################################
#
# python-hidapi
#
################################################################################

PYTHON_HIDAPI_VERSION = 0.14.0.post2
PYTHON_HIDAPI_SOURCE = hidapi-$(PYTHON_HIDAPI_VERSION).tar.gz
PYTHON_HIDAPI_SITE = https://files.pythonhosted.org/packages/bf/6f/90c536b020a8e860f047a2839830a1ade3e1490e67336ecf489b4856eb7b
PYTHON_HIDAPI_SETUP_TYPE = setuptools
PYTHON_HIDAPI_LICENSE = MIT
PYTHON_HIDAPI_LICENSE_FILES = LICENSE

$(eval $(python-package))
