  Licensed to the Apache Software Foundation (ASF) under one or more
  contributor license agreements.  See the NOTICE file distributed with
  this work for additional information regarding copyright ownership.
  The ASF licenses this file to You under the Apache License, Version 2.0
  (the "License"); you may not use this file except in compliance with
  the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

README for tomcat-connectors (mod_jk)
=====================================

What is tomcat-connector?
-------------------------

tomcat-connectors is a project that provides web server connectors
for the Tomcat servlet engine. The supported web servers are the
Apache HTTP Server 1.3 and 2.x, Microsoft IIS and the Netscape/IPlanet
web server. The AJP protocol used by the connector is supported in
all Tomcat versions starting with Tomcat 3.2. Some other back end servers
also support the AJP protocol.

Main features of the tomcat-connectors are fault tolerance, load balancing,
dynamic configuration, flexibility and robustness.

The project was based on the original mod_jk code and keeps maintaining it.
Originally the project also provided the Java parts of the connectors
used inside Tomcat. These parts have since then been moved directly
into the Tomcat source.

How do i build it?
------------------

Just take a look at BUILDING.txt

Where are the docs?
-------------------

In the docs directory.
