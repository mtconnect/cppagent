<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:fn="http://www.w3.org/2005/xpath-functions" xmlns:m="urn:mtconnect.org:MTConnectDevices:1.7" xmlns:s="urn:mtconnect.org:MTConnectStreams:1.7" xmlns:e="urn:mtconnect.org:MTConnectError:1.7" xmlns:js="urn:custom-javascript" exclude-result-prefixes="msxsl js" xmlns:msxsl="urn:schemas-microsoft-com:xslt">

  <xsl:output method="html" version="1.0" encoding="UTF-8" indent="yes" />

  <!-- Root template -->
  <xsl:template match="/">

    <head>
      <meta charset="utf-8"></meta>
      <meta http-equiv="X-UA-Compatible" content="IE=edge"></meta>
      <meta name="viewport" content="width=device-width, initial-scale=1"></meta>
      <title>MTConnect Agent</title>
      <link href="/styles/bootstrap.min.css" rel="stylesheet"></link>
      <link href="/styles/styles.css" rel="stylesheet"></link>
    </head>

    <body>

      <!-- Header with logo, navigation, form and buttons -->
      <nav class="navbar navbar-inverse navbar-fixed-top">
        <div class="container-fluid">
          <div class="navbar-header">
            <button type="button" class="navbar-toggle collapsed" data-toggle="collapse" data-target="#bs-example-navbar-collapse-1" aria-expanded="false">
              <span class="sr-only">Toggle navigation</span>
              <span class="icon-bar"></span>
              <span class="icon-bar"></span>
              <span class="icon-bar"></span>
            </button>
            <a class="navbar-brand" style="padding: 5px 10px;" href="https://mtconnect.org" target="_blank" rel="noopener noreferrer">
              <img alt="Brand" src="styles/LogoMTConnect.gif" />
            </a>
          </div>
          <div class="collapse navbar-collapse" id="bs-example-navbar-collapse-1">
            <ul class="nav navbar-nav">
              <li id="tab-probe">
                <a href="../probe">Probe</a>
              </li>
              <li id="tab-current">
                <a href="../current">Current</a>
              </li>
              <li id="tab-sample">
                <a href="../sample">Sample</a>
              </li>
            </ul>
            <form class="navbar-form navbar-left" onsubmit="return false;">
              <div class="form-group">
                <input id="path" type="text" class="form-control" style="width: 18em; margin-right: 6px;" placeholder="Path" />
                <input id="from" type="text" class="form-control" style="width: 6em; margin-right: 6px;" placeholder="From" />
                <input id="count" type="text" class="form-control" style="width: 6em; margin-right: 6px;" placeholder="Count" />
                <button type="submit" onclick="fetchData()" class="btn" style="margin-right: 6px;">Fetch</button>
                <button id="autorefresh" type="button" class="btn" style="margin-right: 6px;">Autorefresh</button>
                <button id="xml" onclick="showXml()" type="button" class="btn" style="margin-right: 6px;">XML</button>
                <button type="button" class="btn" data-toggle="modal" data-target="#myModal" style="margin-right: 6px;">?</button>
              </div>
            </form>
          </div>
        </div>
      </nav>

      <!-- Main contents -->
      <div class="container-fluid page-container">
        <xsl:apply-templates />
      </div>

      <!-- Modal -->
      <div class="modal fade" id="myModal" tabindex="-1" role="dialog" aria-labelledby="myModalLabel">
        <div class="modal-dialog modal-dialog-centered" role="document">
          <div class="modal-content">
            <div class="modal-header">
              <button type="button" class="close" data-dismiss="modal" aria-label="Close">
                <span aria-hidden="true">x</span>
              </button>
              <h3 class="modal-title" id="myModalLabel">MTConnect Agent</h3>
            </div>
            <div class="modal-body">

              <p>The MTConnect Agent receives data from one or more devices and makes it available as XML, then transforms that into HTML.</p>

              <h4>Probe</h4>
              <p>The Probe tab shows the structure of the available devices and their data items.</p>

              <h4>Current</h4>
              <p>The Current tab shows the latest values for each data item, along with a timestamp.</p>

              <h4>Sample</h4>
              <p>The Sample tab shows a sequence of values and their timestamps.</p>

              <h4>Path</h4>
              <p>
                You can search the available data using a simple query language called
                <a href="https://en.wikipedia.org/wiki/XPath">XPath</a>
                , e.g.
              </p>
              <p>Note: these will give an error if the given path cannot be found in the document.</p>
              <ul>
                <li>
                  Availability of all devices:
                  <a href='../current?path=//DataItem[@type="AVAILABILITY"]'>//DataItem[@type="AVAILABILITY"]</a>
                </li>
                <li>
                  All condition statuses:
                  <a href='../current?path=//DataItems/DataItem[@category="CONDITION"]'>//DataItems/DataItem[@category="CONDITION"]</a>
                </li>
                <li>
                  All controller data items:
                  <a href='../current?path=//Controller/*'>//Controller/*</a>
                </li>
                <li>
                  All linear axis data items:
                  <a href='../current?path=//Axes/Components/Linear/DataItems'>//Axes/Components/Linear/DataItems</a>
                </li>
                <li>
                  X-axis data items:
                  <a href='../current?path=//Axes/Components/Linear[@id="x"]'>//Axes/Components/Linear[@id="x"]</a>
                </li>
                <li>
                  Door status:
                  <a href='../current?path=//Door'>//Door</a>
                </li>
              </ul>

              <h4>From/Count</h4>
              <p>
                The MTConnect Agent stores a certain number of observations, called sequences. You can specify which ones and how many to view with the
                <b>From</b>
                and
                <b>Count</b>
                fields.
              </p>

              <h4>Fetch</h4>
              <p>Click Fetch to retrieve Current or Sample data - if From or Count values are supplied, the relevant Sample data will be shown.</p>

              <h4>Autorefresh</h4>
              <p>Toggles autorefresh of page every 2 seconds.</p>

              <h4>XML</h4>
              <p>Click the XML button to open the underlying raw XML in a new tab.</p>

              <h4>Note</h4>
              <p>Conditions are transformed from Normal, Warning, Fault elements to Condition elements with value of NORMAL, WARNING, FAULT.</p>

              <h4>About</h4>
              <p>
                For more information on MTConnect, see
                <a href="https://mtconnect.org">MTConnect.org</a>
                .
              </p>
              <p>
                This UI was developed by
                <a href="https://mriiot.com">MRIIOT</a>
                , your agile digital transformation partner.
              </p>
              <p>License: MIT</p>
            </div>
            <div class="modal-footer">
              <button type="button" class="btn btn-default" data-dismiss="modal">Close</button>
            </div>
          </div>
        </div>
      </div>

      <button onclick="gotoTop()" id="gotoTop" title="Go to top">
        <img src="/styles/icon-up.gif" />
      </button>

      <!-- note: jquery is needed for bootstrap modal -->
      <script src="/styles/jquery-1.12.4.min.js"></script>
      <script src="/styles/bootstrap.min.js"></script>
      <script src="/styles/script.js"></script>

    </body>
  </xsl:template>

  <!-- include other templates -->
  <!-- Windows can't handle these include statements - see readme -->
  <!-- <xsl:include href="styles-probe.xsl" /> -->
  <!-- <xsl:include href="styles-streams.xsl" /> -->
  <!-- <xsl:include href="styles-error.xsl"/> -->

  <!-- probe -->

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
            <xsl:variable name="element" select="local-name()" />

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
                    <xsl:value-of select="substring('xxxxxxxxxxxx',1,$indent)" />
                  </span>

                  <!-- add +/- if item has any child elements -->
                  <xsl:choose>
                    <xsl:when test="*">
                      <img style="width:12px;" src="/styles/icon-minus.gif" />
                    </xsl:when>
                    <xsl:otherwise>
                      &#8198;
                    </xsl:otherwise>
                  </xsl:choose>
                  &#8198;
                  <!-- narrow space - see https://stackoverflow.com/questions/8515365/are-there-other-whitespace-codes-like-nbsp-for-half-spaces-em-spaces-en-space -->
                  &#8198;
                  <xsl:value-of select="$element" />
                </div>
              </td>

              <td>
                <xsl:value-of select="@id" />
              </td>
              <td>
                <xsl:value-of select="@name" />
              </td>
              <td>
                <xsl:value-of select="@category" />
              </td>
              <td>
                <xsl:value-of select="@type" />
              </td>
              <td>
                <xsl:value-of select="@subType" />
              </td>
              <td>
                <xsl:value-of select="@units" />
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
                            <xsl:value-of select="name()" />
                          </th>
                        </xsl:for-each>
                      </thead>
                      <tbody>
                        <tr>
                          <xsl:for-each select="@*">
                            <td>
                              <xsl:value-of select="." />
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
                            <xsl:value-of select="@uuid" />
                          </td>
                          <td>
                            <xsl:value-of select="@mtconnectVersion" />
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
                            <xsl:value-of select="@uuid" />
                          </td>
                          <td>
                            <xsl:value-of select="@sampleInterval" />
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
                            <xsl:value-of select="name()" />
                          </th>
                        </xsl:for-each>
                        <th>value</th>
                      </thead>
                      <tbody>
                        <tr>
                          <xsl:for-each select="@*">
                            <td>
                              <xsl:value-of select="." />
                            </td>
                          </xsl:for-each>
                          <td>
                            <xsl:value-of select="text()" />
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

  <!-- streams -->

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
                  <xsl:value-of select="@key" />
                </xsl:when>
                <xsl:otherwise>
                  <xsl:value-of select="@sequence" />
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
                  <xsl:value-of select="substring('xxxxxxxxxxxx',1,$indent)" />
                </span>

                <!-- add +/- if item has any child elements -->
                <xsl:choose>
                  <xsl:when test="*">
                    <img style="width:12px;" src="/styles/icon-minus.gif" />
                  </xsl:when>
                  <xsl:otherwise>
										&#8198;
                  </xsl:otherwise>
                </xsl:choose>
                &#8198;
                <!-- narrow space - see https://stackoverflow.com/questions/8515365/are-there-other-whitespace-codes-like-nbsp-for-half-spaces-em-spaces-en-space -->
                &#8198;
                <xsl:value-of select="$element" />
              </td>

              <!-- . and truncate with ellipsis -->
              <!-- <td style="max-width:12em;"> -->
              <td>
                <xsl:value-of select="@dataItemId" />
              </td>
              <td>
                <xsl:value-of select="@name" />
              </td>
              <td>
                <!-- <xsl:value-of select="@timestamp"/> -->
                <!-- <xsl:value-of select="translate(@timestamp,'T',' ')"/> -->
                <!-- <xsl:value-of select="substring(@timestamp,12)"/> -->
                <!-- <xsl:value-of select="substring(@timestamp,12,10)"/> -->
                <span title="{@timestamp}">
                  <xsl:value-of select="substring(@timestamp,12,10)" />
                </span>
              </td>
              <td style="{$sequenceStyle};">
                <!-- <xsl:value-of select="@sequence"/> -->
                <xsl:value-of select="$sequence" />
              </td>
              <td style="{$valueStyle};">
                <xsl:value-of select="$value" />
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
                            <xsl:value-of select="name()" />
                          </th>
                        </xsl:for-each>
                      </thead>
                      <tbody>
                        <tr>
                          <xsl:for-each select="@*">
                            <td>
                              <xsl:value-of select="." />
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