<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet version="1.0"
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:xs="http://www.w3.org/2001/XMLSchema"
	xmlns:fn="http://www.w3.org/2005/xpath-functions"
	xmlns:m="urn:mtconnect.org:MTConnectDevices:1.7"
	xmlns:s="urn:mtconnect.org:MTConnectStreams:1.7"
	xmlns:js="urn:custom-javascript" exclude-result-prefixes="msxsl js"
	xmlns:msxsl="urn:schemas-microsoft-com:xslt">

	<xsl:template match="m:MTConnectDevices">
		<div id="main-container" class="table-responsive stickytable-container">
			<table class="table table-hover">
				<thead>
					<th>Element</th>
					<th>Id</th>
					<th>Name</th>
					<th>Category</th>
					<th>Type</th>
					<th>SubType</th>
					<th>Units</th>
				</thead>
				<tbody>
					<xsl:for-each select="//*">
						<xsl:variable name="indent" select="count(ancestor::*)" />
						<xsl:variable name="element" select="local-name()"/>

						<xsl:variable name="rowStyle">
							<xsl:choose>
								<xsl:when test="$element='Header' or $element='Agent' or $element='Device'">
								  font-weight:bold;
								</xsl:when>
							</xsl:choose>
						</xsl:variable>

						<tr style="{$rowStyle}">

							<td>
								<div class="truncate">
									<!-- indent according to depth and show type, eg Device, DataItem -->
									<span style="color:transparent">
										<xsl:value-of select="substring('xxxxxxxxxxxx',1,$indent)"/>
									</span>

									<!-- add +/- if item has any child elements -->
									<xsl:choose>
										<xsl:when test="*">
											<img style="width:12px;" src="/styles/icon-minus.png" />
										</xsl:when>
										<xsl:otherwise>
										&#8198; 																																																																																																																																																																																																																																																																																																																																																					<!-- space -->
										</xsl:otherwise>
									</xsl:choose>

									<!-- narrow space - see https://stackoverflow.com/questions/8515365/are-there-other-whitespace-codes-like-nbsp-for-half-spaces-em-spaces-en-space -->
								&#8198;
									<xsl:value-of select="$element" />
								</div>
							</td>

							<td>
								<xsl:value-of select="@id"/>
							</td>
							<td>
								<xsl:value-of select="@name"/>
							</td>
							<td>
								<xsl:value-of select="@category"/>
							</td>
							<td>
								<xsl:value-of select="@type"/>
							</td>
							<td>
								<xsl:value-of select="@subType"/>
							</td>
							<td>
								<xsl:value-of select="@units"/>
							</td>
						</tr>

						<!-- Header subtable -->
						<xsl:choose>
							<xsl:when test="$element='Header'">
								<tr>
									<td colspan="7">
										<table class="subtable">
											<thead>
												<xsl:for-each select="@*">
													<th>
														<xsl:value-of select="name()"/>
													</th>
												</xsl:for-each>
											</thead>
											<tbody>
												<tr>
													<xsl:for-each select="@*">
														<td>
															<xsl:value-of select="."/>
														</td>
													</xsl:for-each>
												</tr>
											</tbody>
										</table>
									</td>
								</tr>
							</xsl:when>
						</xsl:choose>

						<!-- Agent subtable -->
						<xsl:choose>
							<xsl:when test="$element='Agent'">
								<tr>
									<td colspan="3">
										<table class="subtable">
											<thead>
												<th>uuid</th>
												<th>mtconnectVersion</th>
											</thead>
											<tbody>
												<tr>
													<td>
														<xsl:value-of select="@uuid"/>
													</td>
													<td>
														<xsl:value-of select="@mtconnectVersion"/>
													</td>
												</tr>
											</tbody>
										</table>
									</td>
								</tr>
							</xsl:when>
						</xsl:choose>

						<!-- Device subtable -->
						<xsl:choose>
							<xsl:when test="$element='Device'">
								<tr>
									<td colspan="3">
										<table class="subtable">
											<thead>
												<th>uuid</th>
												<th>sampleInterval</th>
											</thead>
											<tbody>
												<tr>
													<td>
														<xsl:value-of select="@uuid"/>
													</td>
													<td>
														<xsl:value-of select="@sampleInterval"/>
													</td>
												</tr>
											</tbody>
										</table>
									</td>
								</tr>
							</xsl:when>
						</xsl:choose>

						<!-- Description subtable -->
						<xsl:choose>
							<xsl:when test="$element='Description'">
								<tr>
									<td colspan="7">
										<table class="subtable">
											<thead>
												<xsl:for-each select="@*">
													<th>
														<xsl:value-of select="name()"/>
													</th>
												</xsl:for-each>
												<th>value</th>
											</thead>
											<tbody>
												<tr>
													<xsl:for-each select="@*">
														<td>
															<xsl:value-of select="."/>
														</td>
													</xsl:for-each>
													<td>
														<xsl:value-of select="text()"/>
													</td>
												</tr>
											</tbody>
										</table>
									</td>
								</tr>
							</xsl:when>
						</xsl:choose>

					</xsl:for-each>
				</tbody>
			</table>
		</div>
	</xsl:template>
</xsl:stylesheet>
