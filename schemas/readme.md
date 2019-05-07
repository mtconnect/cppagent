MTConnect Schema Files Versions 1.0 - 1.3
===

Files are named with respect to the section of the standard they apply. 

  ```MTConnect<Part>_<Version>[_<XSD Schema Version>].xsd```
    
The files included in this directory are as follows:

Version 1.0
---

```
  MTConnectDevices_1.0.xsd
  MTConnectStreams_1.0.xsd
  MTConnectError_1.0.xsd
```

Version 1.1
---

```
  MTConnectDevices_1.1.xsd
  MTConnectStreams_1.1.xsd
  MTConnectError_1.1.xsd
```

Version 1.2
---

```
  MTConnectAssets_1.2.xsd
  MTConnectDevices_1.2.xsd
  MTConnectStreams_1.2.xsd
  MTConnectError_1.2.xsd
```

Version 1.3 (With XSD 1.0 compatible files)
---

```
  MTConnectAssets_1.3.xsd
  MTConnectAssets_1.3_1.0.xsd
  MTConnectDevices_1.3.xsd
  MTConnectDevices_1.3_1.0.xsd
  MTConnectStreams_1.3.xsd
  MTConnectStreams_1.3_1.0.xsd
  MTConnectError_1.3.xsd
  MTConnectError_1.3_1.0.xsd
```

Microsoft XML an many legacy XML parsers are not current on the XML Schema 1.1 standard accepted in 2012 by the w3c. The MTConnect standard takes advantage of the latest advances in extensibility to add additional properties in a regulated manor using the xs:any tag and specifying they tags must be from another namespace.

We are also using Schema Versioning from XML Schema 1.1 and will be creating new schema that use these new features as we move into 1.4 and beyond. There are many XML Parsers that now correctly handle XML Schema 1.1, namely Raptor from Altova and Xerces from Apache. If you must use Microsoft XML with validation turned on, then you must use the ...1.3_1.0.xsd files in the directory. 

The 1.3_1.0.xsd files will support the MSXML parser and validation, but will not support the advance extensibility. If you want both, talk with Microsoft to update their parser.