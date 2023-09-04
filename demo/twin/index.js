import * as THREE from './lib/threejs/three.module.js';

import { OrbitControls } from './lib/threejs/jsm/controls/OrbitControls.js';
import { RestProtocol } from './lib/mtconnect/protocol.js';
import { VRButton } from './lib/threejs/jsm/webxr/VRButton.js';


let camera, scene, renderer, stats;

// any custom display data specified in data.txt
let customData = new Map();
let conditions = new Map();
let displayElements = [];
let deviceName = '';

document.onreadystatechange = function() {
  if (document.readyState !== "complete") {
    document.querySelector("#loader").style.visibility = "visible";
    document.querySelector("#loader_text").style.visibility = "visible";
  }
};

if (window.location.search.length > 1)
{
  const fields = Object.fromEntries([window.location.search.substr(1).split('=')]);
  if (fields.device) {
    deviceName = fields.device;
  }
}

init();

loadAndStreamData();

onWindowResize()

function init() {
  document.title = "MTConnect Demo by Metalogi.io";

  THREE.Object3D.DefaultUp = new THREE.Vector3(0,0,1);

  const container = document.createElement( 'div' );
  document.body.appendChild( container );

  setupCustomData();

  camera = new THREE.PerspectiveCamera( 45, window.innerWidth / window.innerHeight, 1, 100 );
  camera.position.set( 0.9, -5.5, 3.5 );

  scene = new THREE.Scene();
  scene.background = new THREE.Color( 0xa0a0a0 );

  const ambientLight = new THREE.AmbientLight( 0x222222 );
  scene.add( ambientLight );

  const light1 = new THREE.DirectionalLight( 0x505050 );
  light1.position.set( 0, -20, 10 );
  scene.add( light1 );

  const grid = new THREE.GridHelper( 10, 50, 0x000000, 0x000000 );
  grid.rotateX(Math.PI / 2); 
  grid.material.opacity = 0.3;
  grid.material.transparent = true;
  scene.add( grid );

  renderer = new THREE.WebGLRenderer( { antialias: true } );
  renderer.setPixelRatio( window.devicePixelRatio );
  renderer.setSize( window.innerWidth, window.innerHeight );
  setInterval(() => requestAnimationFrame( animate ), 34);
  renderer.outputEncoding = THREE.sRGBEncoding;
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.xr.enabled = true;
  container.appendChild( renderer.domElement );
  container.appendChild( VRButton.createButton( renderer ) );

  const controls = new OrbitControls( camera, renderer.domElement );
  controls.target.set( 0.0, 0.0, 1.0);
  controls.update();

  window.addEventListener( 'resize', onWindowResize );  

  renderer.setAnimationLoop( function () {
    renderer.render( scene, camera );
  });
}

async function loadAndStreamData() {
  const protocol = new RestProtocol('/' + deviceName, values => {
    for (let [dataItem, obs] of values) {
      const did = dataItem.id;
      let v = obs.value;
      let ele = document.getElementById(did);
      if (typeof v == 'number')
        v = v.toFixed(3);

      if (ele) 
        ele.textContent=k + ": " + v;

      let cell = customData.get(did);
      if (cell)
        cell.nodeValue = v;

      // Active Alarms in dataItem.value
      let obj = conditions.get(did);
      if (obj) {
        var span = obj.span;
        if (Array.isArray(dataItem.value) && dataItem.value.length == 0) {
          if (span.textContent != '') {
            span.textContent = obj.label+': Normal';
            span.setAttribute('style', 'padding-left:25px; color: white');
          }
        } else if (Array.isArray(dataItem.value)) {
          const text = dataItem.value.map ( alarm => {
            return alarm.level + (alarm.value ? ': "' + alarm.value + '"' : '');
          }).join(', ');
          span.textContent = obj.label + ': ' + text;
          span.setAttribute('style', 'padding-left:25px; color: yellow');
        }
      }
    }
  });

  const device = await protocol.probeMachine();
  const machineCS = device.findCoordinateSystem('MACHINE')[0];

  const dataItems = device.findDataItems(di => {
    return (((di.type == 'POSITION' || di.type == 'ANGLE') && di.subType == 'ACTUAL') && di.coordinateSystemIdRef == machineCS.id) ||
      customData.has(di.id) || 
      di.category == 'CONDITION' ||
            di.type == 'AVAILABILITY' || di.type == 'CONTROLLER_MODE' || di.type == 'EXECUTION' || 
            (di.type == 'PROGRAM' && di.subType == 'MAIN');
  });

  const conditionDataItems = device.findDataItems(di => {
    return (di.category == 'CONDITION');
  });

  setupConditionDisplay(conditionDataItems);
  protocol.loadModel(obj => {
    const enc = device.findComponent('Enclosure');
    const material = new THREE.MeshPhongMaterial( { color: 0x0A0A0A, specular: 0x111111, shininess: 200, opacity: 0.5, transparent: true } );
                if (enc)
      enc.geometry.model.material = material;
    
    protocol.streamData(dataItems.concat(conditionDataItems));

    scene.add( obj.model );

    document.querySelector("#loader").style.display = "none";
    document.querySelector("#loader_text").style.display = "none";
  });   
}

function determineSize() {
  let size = Math.floor(window.innerWidth / 100);
  console.log('size', size);
  return size;
}

function onWindowResize() {

  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();

  renderer.setSize( window.innerWidth, window.innerHeight );

  let size = determineSize();
  displayElements.forEach(row => {
    row.setAttribute('style', 'line-height: '+size+'px; font-size: '+size+'px;');
  })

  size *= 3;
  let title = document.getElementById("title");
  title.textContent = 'Demo by Metalogi.io';
  title.setAttribute("style", "font-size: " + size + "px;");
}

function animate() {
  renderer.render( scene, camera )
}

function setupCustomData() {
  fetch('./data_' + deviceName + '.txt')
    .then( resp => resp.text())
    .then( text => {
    // avoid load the data more than once
    if (customData.size > 0)
      return;

    let size = determineSize();
    var body = document.getElementsByTagName('body')[0];

      // creates a <table> element and a <tbody> element
    var tbl = document.createElement('table');
    var tblBody = document.createElement('tbody');
    for (const line of text.split('\n').map( l => l.trim())) {
      if (line.startsWith('#'))
        continue;
      
      var pair = line.split('|');
      var dataId = pair[0].trim();
      if (dataId == '')
        continue;

      // must be a pair
      if (pair.length >= 2) {
        // creates a table row
        var row = document.createElement('tr');

        let r
        for (var j = 0; j < 2; j++) {

          // col 0 - label text
          // col 1 - real-time data
          var cell = document.createElement('td');
          var displayLabel = pair[1].trim();
          if (j == 0) {
            var displayLabel = pair[1].trim();
            if (displayLabel == '') displayLabel = dataId;

            var cellText = document.createTextNode(displayLabel); 
            cell.appendChild(cellText);
          } else {
            var cellText = document.createTextNode(''); 
            cell.appendChild(cellText);
            customData.set(dataId, cellText);
            cell.style.color = 'yellow';
          }
          row.appendChild(cell);
        }

        row.setAttribute('style', 'line-height: '+size+'px; font-size: '+size+'px;');
        displayElements.push(row);
        // add the row to the end of the table body
        tblBody.appendChild(row);
      }
    }

      // put the <tbody> in the <table>
      tbl.appendChild(tblBody);

      // appends <table> into <body>
      body.appendChild(tbl);
      tbl.setAttribute('class', 'data_table');
  });
}

function PascalCase(str) {
  return  (" " + str).toLowerCase().replace(/[^a-zA-Z0-9]+(.)/g, function(match, chr) {
    return chr.toUpperCase();
  });
}

function setupConditionDisplay(conditionDataItems) {
  var body = document.getElementsByTagName('body')[0];

  var alarms = document.createElement('p');
  alarms.setAttribute('class', 'marquee');

  const container = document.createElement( 'div' );
  container.setAttribute('style', 'padding-left: 100%; animation: marquee 20s linear infinite');

  // create a span for each condition
  conditionDataItems.forEach(data => {
    var child = document.createElement('span');
    child.setAttribute('id', 'span_' + data.id);

    var subtype = data.subtype ? PascalCase(data.subtype) : '';
    var name = data.name? '['+data.name+']' : '';
    if (name == '')
        name = '[' + data.id + ']';
    conditions.set(data.id, { label: subtype+PascalCase(data.type)+name, span: child });
      container.appendChild(child);
  });

  alarms.appendChild(container);
  body.appendChild(alarms);
}
