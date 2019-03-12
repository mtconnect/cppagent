<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
	<xsl:output method="xml" indent="yes"/>
	<xsl:template match="/">
		<testsuite>
			<xsl:attribute name="errors">
				<xsl:value-of select="TestRun/Statistics/Errors"/>
			</xsl:attribute>
			<xsl:attribute name="failures">
				<xsl:value-of select="TestRun/Statistics/Failures"/>
			</xsl:attribute>
			<xsl:attribute name="tests">
				<xsl:value-of select="TestRun/Statistics/Tests"/>
			</xsl:attribute>
			<xsl:attribute name="name">from cppunit</xsl:attribute>
			<xsl:apply-templates/>
		</testsuite>
	</xsl:template>
	<xsl:template match="/TestRun/SuccessfulTests/Test">
		<testcase>
			<xsl:attribute name="classname" ><xsl:value-of select="substring-before(Name, '::')"/></xsl:attribute>
			<xsl:attribute name="name"><xsl:value-of select="substring-after(Name, '::')"/></xsl:attribute>
		</testcase>
	</xsl:template>
	<xsl:template match="/TestRun/FailedTests/FailedTest">
		<testcase>
			<xsl:attribute name="classname" ><xsl:value-of select="substring-before(Name, '::')"/></xsl:attribute>
			<xsl:attribute name="name"><xsl:value-of select="substring-after(Name, '::')"/></xsl:attribute>
			<error>
				<xsl:attribute name="message">
					<xsl:value-of select=" normalize-space(Message)"/>
				</xsl:attribute>
				<xsl:attribute name="type">
					<xsl:value-of select="FailureType"/>
				</xsl:attribute>
				<xsl:value-of select="Message"/>
				File:<xsl:value-of select="Location/File"/>
				Line:<xsl:value-of select="Location/Line"/>			
			</error>
		</testcase>
	</xsl:template>
  <!-- skip all text -->
	<xsl:template match="text()|@*"/>
</xsl:stylesheet><!-- Stylus Studio meta-information - (c)1998-2001 eXcelon Corp.
<metaInformation>
<scenarios ><scenario default="yes" name="test" userelativepaths="yes" url="..\..\..\..\..\Tmp\xml\cppunit.xml" htmlbaseurl="" processortype="internal" commandline="" additionalpath="" additionalclasspath="" postprocessortype="none" postprocesscommandline="" postprocessadditionalpath="" postprocessgeneratedext=""/></scenarios><MapperInfo  srcSchemaPath="..\..\..\..\..\Tmp\xml\cppunit.xml" srcSchemaRoot="TestRun" srcSchemaPathIsRelative="yes" destSchemaPath="..\..\..\..\..\Tmp\xml\TEST&#x2D;test.osmoose.license.TestUtils.xml" destSchemaRoot="testsuite" destSchemaPathIsRelative="yes" />
</metaInformation>
-->