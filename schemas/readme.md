MTConnect Schema Files Versions 1.0 - 2.5
===

Files are named with respect to the section of the standard they apply. 

  ```MTConnect<Part>_<Version>[_<XSD Schema Version>].xsd```
    
The files included in this directory are as follows:

* Version 1.0
* Version 1.1
* Version 1.2
* Version 1.3 (With XSD 1.0 compatible files)
* Version 1.4 (With XSD 1.0 compatible files)
* Version 1.5 (With XSD 1.0 compatible files)
* Version 1.6 (With XSD 1.0 compatible files)
* Version 1.7 (With XSD 1.0 compatible files)
* Version 1.8 (With XSD 1.0 compatible files)
* Version 2.0 (With XSD 1.0 compatible files)
* Version 2.1 (With XSD 1.0 compatible files)
* Version 2.2 (With XSD 1.0 compatible files)
* Version 2.3 (With XSD 1.0 compatible files)
* Version 2.4 (With XSD 1.0 compatible files)
* Version 2.5 (With XSD 1.0 compatible files)

The schemas are replicated to http://schemas.mtconnect.org

Microsoft XML an many legacy XML parsers are not current on the XML Schema 1.1 standard accepted in 2012 by the w3c. The MTConnect standard takes advantage of the latest advances in extensibility to add additional properties in a regulated manor using the xs:any tag and specifying they tags must be from another namespace.

We are also using Schema Versioning from XML Schema 1.1 and will be creating new schema that use these new features as we move into 1.4 and beyond. There are many XML Parsers that now correctly handle XML Schema 1.1, namely Raptor from Altova and Xerces from Apache. If you must use Microsoft XML with validation turned on, then you must use the ...1.3_1.0.xsd files in the directory. 

The 1.3_1.0.xsd files will support the MSXML parser and validation, but will not support the advance extensibility. If you want both, talk with Microsoft to update their parser.
