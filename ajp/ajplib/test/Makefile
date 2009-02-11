#
#  Licensed to the Apache Software Foundation (ASF) under one or more
#  contributor license agreements.  See the NOTICE file distributed with
#  this work for additional information regarding copyright ownership.
#  The ASF licenses this file to You under the Apache License, Version 2.0
#  (the "License"); you may not use this file except in compliance with
#  the License.  You may obtain a copy of the License at
# 
#      http://www.apache.org/licenses/LICENSE-2.0
# 
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#

#
# You need an installed httpd-2.x to use this Makefile
#

APACHE_DIR=/home2/apache20/apache20
APR_DIR=$(APACHE_DIR)
APU_DIR=$(APACHE_DIR)

include $(APR_DIR)/build/config_vars.mk

# in httpd-2.x sources...
#APR_DIR=$(APACHE_DIR)/srclib/apr
#APU_DIR=$(APACHE_DIR)/srclib/apr-util

APR_INCLUDE=$(APR_DIR)/include
APU_INCLUDE=$(APU_DIR)/include
APA_INCLUDE=$(APACHE_DIR)/include

SRC_DIR=.
INCLUDE=.
AJP_OBJECTS = ajp_link.lo  ajp_msg.lo  httpd_wrap.lo ajp_header.lo
AJP_PROGRAM = testajp.lo
AJP_LIB     = lib_ajp.la

INCLUDES = -I $(INCLUDE) -I $(APR_INCLUDE) -I $(APU_INCLUDE) -I $(APA_INCLUDE)

include $(APR_DIR)/build/apr_rules.mk

all: testajp

testajp: $(AJP_PROGRAM) $(AJP_LIB)
	$(LINK) $(AJP_PROGRAM) $(AJP_LIB) $(AP_LIBS)

$(AJP_LIB): $(AJP_OBJECTS)
	$(LINK) $(AJP_OBJECTS)

clean:
	rm -f *.o *.so *.lo *.la *.slo
	rm -rf .libs
	rm -f testajp
