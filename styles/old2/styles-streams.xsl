<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xs="http://www.w3.org/2001/XMLSchema"
  xmlns:fn="http://www.w3.org/2005/xpath-functions"
  xmlns:m="urn:mtconnect.org:MTConnectDevices:1.7"
  xmlns:s="urn:mtconnect.org:MTConnectStreams:1.7"
  xmlns:js="urn:custom-javascript" exclude-result-prefixes="msxsl js"
  xmlns:msxsl="urn:schemas-microsoft-com:xslt">

  <xsl:template match="s:MTConnectStreams">
    <div id="main-container" class="table-responsive stickytable-container">
      <table class="table table-hover">
        <thead>
          <th>Element</th>
          <th>Id</th>
          <th>Name</th>
          <th>Timestamp</th>
          <th>Sequence</th>
          <th>Value</th>
        </thead>
        <tbody>
          <xsl:for-each select="//*">
            <xsl:variable name="indent" select="count(ancestor::*)" />

            <!-- get element name -->
            <xsl:variable name="element">
              <xsl:choose>
                <xsl:when test="local-name()='Condition'">Conditions</xsl:when>
                <xsl:when test="local-name()='Normal'">Condition</xsl:when>
                <xsl:when test="local-name()='Warning'">Condition</xsl:when>
                <xsl:when test="local-name()='Fault'">Condition</xsl:when>
                <xsl:when test="local-name()='Unavailable'">Condition</xsl:when>
                <xsl:when test="local-name()='Entry'"></xsl:when>
                <xsl:otherwise>
                  <xsl:value-of select="local-name()" />
                </xsl:otherwise>
              </xsl:choose>
            </xsl:variable>

            <!-- get style for the row -->
            <xsl:variable name="rowStyle">
              <xsl:choose>
                <xsl:when test="$element='Header' or $element='DeviceStream' or $element='Samples' or $element='Events' or $element='Conditions'">
								  font-weight:bold;
                </xsl:when>
              </xsl:choose>
            </xsl:variable>

            <!-- get value -->
            <xsl:variable name="value">
              <xsl:choose>
                <xsl:when test="local-name()='Normal'">NORMAL</xsl:when>
                <xsl:when test="local-name()='Warning'">WARNING</xsl:when>
                <xsl:when test="local-name()='Fault'">FAULT</xsl:when>
                <xsl:when test="local-name()='Unavailable'">UNAVAILABLE</xsl:when>
                <xsl:otherwise>
                  <xsl:value-of select="text()" />
                </xsl:otherwise>
              </xsl:choose>
            </xsl:variable>

            <!-- get sequence (or entry key) -->
            <xsl:variable name="sequence">
              <xsl:choose>
                <xsl:when test="local-name()='Entry'">
                  <xsl:value-of select="@key"/>
                </xsl:when>
                <xsl:otherwise>
                  <xsl:value-of select="@sequence"/>
                </xsl:otherwise>
              </xsl:choose>
            </xsl:variable>

            <!-- get value style -->
            <!-- . or assign a class and define colors in stylesheet -->
            <xsl:variable name="valueStyle">
              <xsl:choose>
                <xsl:when test="$value='NORMAL'">background: #9fe473</xsl:when>
                <xsl:when test="$value='WARNING'">background: #ffe989</xsl:when>
                <xsl:when test="$value='FAULT'">background: #ff4c41</xsl:when>
                <xsl:when test="$value='AVAILABLE'">background: #e7f38b</xsl:when>
                <xsl:when test="$value='UNAVAILABLE'">color: #aaa</xsl:when>
                <xsl:when test="$value='ACTIVE'">background: #9fe473</xsl:when>
                <xsl:when test="$value='INACTIVE'">background: #f0f0f0</xsl:when>
                <xsl:otherwise></xsl:otherwise>
              </xsl:choose>
            </xsl:variable>

            <!-- get sequence style -->
            <xsl:variable name="sequenceStyle">
              <xsl:choose>
                <xsl:when test="local-name()='Entry'">text-align:right;</xsl:when>
                <xsl:otherwise></xsl:otherwise>
              </xsl:choose>
            </xsl:variable>

            <tr style="{$rowStyle}">

              <td>
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
										&#8198; 																																																																																																																																																																																				                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    <!-- space -->
                  </xsl:otherwise>
                </xsl:choose>

                <!-- narrow space - see https://stackoverflow.com/questions/8515365/are-there-other-whitespace-codes-like-nbsp-for-half-spaces-em-spaces-en-space -->
								&#8198;
                
                <xsl:value-of select="$element" />
              </td>

              <!-- . and truncate with ellipsis -->
              <!-- <td style="max-width:12em;"> -->
              <td>
                <xsl:value-of select="@dataItemId"/>
              </td>
              <td>
                <xsl:value-of select="@name"/>
              </td>
              <td>
                <!-- <xsl:value-of select="@timestamp"/> -->
                <!-- <xsl:value-of select="translate(@timestamp,'T',' ')"/> -->
                <!-- <xsl:value-of select="substring(@timestamp,12)"/> -->
                <!-- <xsl:value-of select="substring(@timestamp,12,10)"/> -->
                <span title="{@timestamp}">
                  <xsl:value-of select="substring(@timestamp,12,10)"/>
                </span>
              </td>
              <td style="{$sequenceStyle};">
                <!-- <xsl:value-of select="@sequence"/> -->
                <xsl:value-of select="$sequence"/>
              </td>
              <td style="{$valueStyle};">
                <xsl:value-of select="$value"/>
              </td>
            </tr>

            <!-- Header subtable -->
            <xsl:choose>
              <xsl:when test="$element='Header'">
                <tr>
                  <td colspan="6">
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

          </xsl:for-each>
        </tbody>
      </table>
    </div>
  </xsl:template>

  <!-- <xsl:template match="s:VariableDataSet">
    <table>
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
  </xsl:template> -->


</xsl:stylesheet>
