<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:fn="http://www.w3.org/2005/xpath-functions" xmlns:m="urn:mtconnect.org:MTConnectStreams:1.3" >
	<xsl:output method="html" version="1.0" encoding="UTF-8" indent="yes"/>
	<xsl:template match="/">
			<head>
				<title>MTConnect Device Streams</title>
				<link type="text/css" href="/styles/Streams.css" media="screen" rel="stylesheet"/>
			</head>
			<body>
				<p>
					<xsl:apply-templates select="/m:MTConnectStreams/m:Header" />
				</p>
				<hr/>
				<xsl:apply-templates select="/m:MTConnectStreams/m:Streams/m:DeviceStream" />
			</body>
	</xsl:template>
	
	<xsl:template match="m:DeviceStream">
		<h2>Device: <xsl:value-of select="@name"/>
		<xsl:text>; UUID: </xsl:text><xsl:value-of select="@uuid"/>
		</h2>
		<xsl:apply-templates select="m:ComponentStream"/>
	</xsl:template>
	
	<xsl:template match="m:Header">
		<ul>
			<xsl:for-each select="@*">
				<li>
					<xsl:value-of select="name()"/>
					<xsl:text>: </xsl:text>
					<xsl:value-of select="." />
				</li>
			</xsl:for-each>
		</ul>
	</xsl:template>
	
	<xsl:template match="m:ComponentStream">
		<h3>
			<xsl:value-of select="@component" />
			<xsl:text> : </xsl:text>
			<xsl:value-of select="@name" />
		</h3>
		<xsl:apply-templates select="	m:Samples"/>
		<xsl:apply-templates select="	m:Events"/>
		<xsl:apply-templates select="	m:Condition"/>
	</xsl:template>
	
	<xsl:template match="*">
		<p><xsl:value-of select="name()"/></p>
		<table border="1px">
			<thead>
				<th>Timestamp</th><th>Type</th><th>Sub Type</th><th>Name</th><th>Id</th>
				<th>Sequence</th><th>Value</th>
			</thead>
			<tbody>
				<xsl:for-each select="*">
					<tr>
						<td><xsl:value-of select="@timestamp"/></td>
						<td><xsl:value-of select="name()"/></td>
						<td><xsl:value-of select="@subType"/></td>
						<td><xsl:value-of select="@name"/></td>
						<td><xsl:value-of select="@dataItemId"/></td>
						<td><xsl:value-of select="@sequence"/></td>
						<td><xsl:value-of select="."/></td>
					</tr>
				</xsl:for-each>
			</tbody>
		</table>
	</xsl:template>
	
</xsl:stylesheet>
