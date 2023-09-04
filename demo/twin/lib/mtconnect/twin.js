
import * as JP from "../jsonpath/jsonpath.js";
import * as THREE from '../threejs/three.module.js';
import { OBJLoader } from '../threejs/jsm/loaders/OBJLoader.js';
import { STLLoader } from '../threejs/jsm/loaders/STLLoader.js';
import { JsonParserV1 } from "./json_parser_v1.js";
import { JsonParserV2 } from "./json_parser_v2.js";

const _radians = new THREE.Vector3(1.0 / 180.0 * Math.PI, 1.0 / 180.0 * Math.PI, 1.0 / 180.0 * Math.PI);
const _meters = new THREE.Vector3(0.001, 0.001, 0.001);

class CoordinateSystem {
  constructor(doc, comp) {
    Object.assign(this, doc);
    this.component = comp;
    comp.entities[this.id] = this;

    if (this.Transformation) {
      if (this.Transformation.Rotation) {
        const rot = this.Transformation.Rotation;
        this.rotatation = new THREE.Euler(rot[0], rot[1], rot[3], 'ZYX');
      }
      
      if (this.Transformation.Translation) {
        const trans = new THREE.Vector3().fromArray(this.Transformation.Translation);
        this.translation = trans;
      }
    }
  }

  translate(vector) {
    if (this.translation) {
      vector.add(this.translation);
    }
    return vector;
  }

  rotate(vector) {
    if (this.rotation) {
      vector.applyEuler(this.rotation);
    }
    return vector;
  }

  shape() {
    if (!this.obj3d) {
      this.geometry = new THREE.BoxGeometry(0.1, 0.1, 0.1 );
      const m1 =  new THREE.Vector3(this.translation.x / 1000.0,
                                    this.translation.y / 1000.0,
                                    this.translation.z / 1000.0);
      const m2 = new THREE.Vector3().copy(m1).negate();
      if (this.translation) {
        //this.geometry.translate(m2.x, m2.y, m2.z);
      }

      this.vector = m1;

      this.matrix = new THREE.Matrix4();
      if (this.rotation) {
        this.matrix.makeRotationFromEuler(this.rotation);
      }
      if (this.translation) {
        this.matrix.setPosition(m1.x, m1.y, m1.z);
      }
      this.geometry.applyMatrix4(this.matrix);
      const edges = new THREE.EdgesGeometry( this.geometry );
      this.obj3d = new THREE.LineSegments( edges, new THREE.LineBasicMaterial( { color: 0xFF0000 } ) );
    }
    return this.obj3d;
  }

  geometry() {
    //shape();
    return this.geometry;
  }

  resolve() {
    if (this.parentIdRef) {
      this.parent = this.component.entities[this.parentIdRef];
    }
  }
}

class Motion {
  constructor(doc, comp) {
    Object.assign(this, doc);
    this.component = comp;
    comp.entities[this.id] = this;

    if (this.Axis) {
      this.axis = new THREE.Vector3().fromArray(this.Axis);
    } else {
      switch (this.component.name[0]) {
        case 'A':
        case 'X': 
          this.axis = new THREE.Vector3(1.0, 0.0, 0.0);
          break;

        case 'B':
        case 'Y': 
          this.axis = new THREE.Vector3(0.0, 1.0, 0.0);
          break;
  
        case 'C':
        case 'Z': 
          this.axis = new THREE.Vector3(0.0, 0.0, 1.0);
          break;
      }
    }

    this.transform();
  }

  transform() {
    this.rotation = null;
    this.translation = null;
    
    if (this.Transformation) {
      if (this.Transformation.Rotation) {
        const rot = this.Transformation.Rotation;
        this.rotatation = new THREE.Euler(rot[0], rot[1], rot[3], 'ZYX');
      }
      
      if (this.Transformation.Translation) {
        const trans = new THREE.Vector3().fromArray(this.Transformation.Translation);        
        this.translation = trans;
      }
    }
  }


  resolve() {
    if (this.parentIdRef) {
      this.parent = this.component.entities[this.parentIdRef];
    }
    if (this.coordinateSystemIdRef) {
      this.coordinateSystem = this.component.entities[this.coordinateSystemIdRef];
    }
  }
}

class Geometry {
  constructor(doc, comp) {
    Object.assign(this, doc);
    this.children = [];
    this.component = comp;
    comp.entities[this.id] = this;

    this.scale = new THREE.Vector3(1.0, 1.0, 1.0);
    if (this.nativeUnits)
    {
      const units = this.nativeUnits.split(':').pop();
      if (units == 'METER') {
        //this.scale = new THREE.Vector3().copy(_meters);
      }
    }
    if (this.Scale) {
      const v = new THREE.Vector3().fromArray(this.Scale);
      this.scale.multiply(v);
    }
  }

  load(complete) {
    if (this.href) {
      let loader;
      if (this.mediaType == 'OBJ') {
        loader = new OBJLoader();
      } else if (this.mediaType == 'STL') {
        loader = new STLLoader();
      }

      loader.load(this.href, obj => {
        if (this.mediaType == 'OBJ') {
          this.model = obj;
        } else if (this.mediaType == 'STL') {
          const material = new THREE.MeshPhongMaterial( { color: 0x0A0A0A, specular: 0x111111, shininess: 200, opacity: 1.0, transparent: false } );
          this.model = new THREE.Mesh( obj, material );
        }
        this.model.scale.copy(this.scale);
        this.transform();

        this.children.forEach(c => {
          c.loadChild();
        });

        complete(this);
      });
    }
  }

  loadChild() {
    if (this.parent && this.itemRef) {
      this.model = this.parent.model.getObjectByName(this.itemRef);
      this.transform();
    }
  }

  transform() {
    if (this.Transformation) {
      if (this.Transformation.Rotation) {
        const rot = this.Transformation.Rotation;
        this.rotate(new THREE.Euler(rot[0], rot[1], rot[3], 'ZYX'));
      }

      if (this.Transformation.Translation) {
        const trans = new THREE.Vector3().fromArray(this.Transformation.Translation);        
        this.position(trans);
      }

      const geometry = new THREE.BoxGeometry(0.01, 0.01, 0.01 );
      const edges = new THREE.EdgesGeometry( geometry );
      const line = new THREE.LineSegments( edges, new THREE.LineBasicMaterial( { color: 0x000000 } ) );
      this.model.add( line );        
    }
  }

  position(pos) {
    const p = pos.multiply(_meters);
    console.log("position", p);
    this.model.position.copy(p);
  }

  rotate(rot) {
    const v = _radians.clone().multiply(rot);

    console.log("rotate", v);
    this.model.rotation.set(v.x, v.y, v.z);
  }

  resolve() {
    if (this.solidModelIdRef) {
      this.parent = this.component.entities[this.solidModelIdRef];
      if (this.parent) {
        this.parent.children.push(this);
      }

      if (!this.Scale) {
        this.scale = this.parent.scale;
      }
    }
    if (this.coordinateSystemIdRef) {
      this.coordinateSystem = this.component.entities[this.coordinateSystemIdRef];
    }
  }
}

class Fixture {
  constructor(dataItem) {
    this.dataItem = dataItem;        
  }

  apply(obs) {
    // Create a SolidModel from the observation
    let solidmodel = new Object();
    Object.assign(solidmodel, obs);
    if (solidmodel.Scale) {
      solidmodel.Scale = solidmodel.Scale.split(/[ ]+/).map(s => parseFloat(s));      
    }
    if (solidmodel.attach) {
      solidmodel.parent = this.dataItem.component.root.findGeometry(solidmodel.attach);
    }  

    if (solidmodel.Translation || solidmodel.Rotation) {
      solidmodel.Transformation = {
        Translation: solidmodel.Translation ? solidmodel.Translation.split(/[ ]+/).map(s => parseFloat(s)) : null,
        Rotation: solidmodel.Rotation ? solidmodel.Rotation.split(/[ ]+/).map(s => parseFloat(s)) : null
      }
    }

    solidmodel.id = this.dataItem.id;
    this.geometry = new Geometry(solidmodel, this.dataItem.component);

    this.geometry.load(geo => {
      if (solidmodel.parent) {
        solidmodel.parent.model.attach(this.geometry.model);
      }
    });
  }
}

class DataItem {
  constructor(doc, comp) {
    Object.assign(this, doc);
    this.component = comp;
    comp.entities[this.id] = this;
    this.value = null;
    this.machine = false;
  }

  resolve() {
    if (this.coordinateSystemIdRef) {
      this.coordinateSystem = this.component.entities[this.coordinateSystemIdRef];
      this.machine = this.coordinateSystem.type == 'MACHINE';
    }
    if (this.component.motion) {
      this.motion = this.component.motion.type;
      this.axis = this.component.motion.axis;
    }
  }

  apply(key, data) {
    if (this.category == 'CONDITION') {
      if (key == 'Unavailable') {
        this.value = null;
      } else {
        console.debug(this.id, ": Got ", key, " with ", data.nativeCode);
        // If normal and no native code, all conditions have been reset
        if (key == 'Normal' && !data.nativeCode) {
          console.debug("Clearing all: ", key);
          this.value = [];
        } else  {
          // If a native code exist, find it and remove the entry since it is either normal
          // or has changed
          if (data.nativeCode && this.value && this.value.length > 0) {
            // see if an alarm with the same native code exists
            const ind = this.value.findIndex(o => {
              return o.nativeCode == data.nativeCode;
            });
            // if found, remove the found alarm from the list
            if (ind >= 0) {
              console.debug("Removing ", this.value[1]);
              this.value.splice(ind, 1);
            }
          }

          console.debug(this.value);
          
          // If this is not a normal, add it to the list of active alarms
          if (key != 'Normal') {
            // If this.value was null, set it to an array
            if (!this.value) this.value = [];
            data.level = key;
            this.value.push(data);
          }
        }
      }
    } else {
      if (data.value == 'UNAVAILABLE') {
        data.value = null;
      }

      this.value = data;
      const value = data.value;

      if (this.machine && ((this.type == 'POSITION' || this.type == 'ANGLE') && this.subType == 'ACTUAL') &&
          typeof value == 'number') {
        const child = this.component.attachChild;
        if (child && child.geometry && this.axis) {
          const vec = this.axis.clone().multiplyScalar(value);
          if (this.type == 'POSITION' && this.component.motion.coordinateSystem) {
            const comp = this.component.motion.coordinateSystem.translation.clone().negate().multiply(this.axis);
            vec.add(comp);
          }
          if (this.motion == 'REVOLUTE') {
            child.geometry.rotate(vec);
          } else if (this.motion == 'PRISMATIC') {
            child.geometry.position(vec);
          }
        }
      } else if (this.type == 'FIXTURE_GEOMETRY' && value) {
        // set the fixture geometry informaiton and attach to the table
        if (!this.fixture) {
          this.fixture = new Fixture(this);
        }
        this.fixture.apply(value);
      }
    }
  }
}

class Component {
  constructor(type, doc, parser, parent = null) {
    Object.assign(this, doc);
    this.link = false;
    this.joint = false;
    this.type = type;
    this.parent = parent;
    this.parse(doc, parser);

    this.entities[this.id] = this;
  }

  get root() {
    if (!this._root) { 
      if (this.parent) {
        this._root = this.parent.root;
      } else {
        this._entities = {};
        this._root = this;
      }
    }
    return this._root;
  }

  get entities() {
    return this.root._entities;
  }

  parse(doc, parser) {
    this.children = parser.components(doc, (t, c) => {
      return new Component(t, c, parser, this);
    });

    if (this.Configuration) {
      const config = this.Configuration;
      if (config.SolidModel) {
        this.geometry = new Geometry(config.SolidModel, this);
      }

      if (config.Relationships) {
        parser.componentRelationships(config.Relationships, rel => {
          const type = rel.type;
          if (type == 'PARENT') {
            this.attachParentId = rel.idRef;
          } else if (type == 'CHILD') {
            this.attachChildId = rel.idRef;
          }
        });
      }
      
      if (config.Motion) { 
        this.motion = new Motion(config.Motion, this);
      }
       
      if (config.CoordinateSystems) {
        this.coordinateSystems = parser.coordinateSystems(config.CoordinateSystems, c => new CoordinateSystem(c, this) );
        console.log(this.coordinateSystems);
      }
    }
    if (this.DataItems) {
      this.dataItems = parser.dataItems(this, d => 
        new DataItem(d, this));
    }
  }

  resolve() {
    for (const c of this.children) {
      c.resolve();
    }
    if (this.attachChildId) {
      this.attachChild = this.entities[this.attachChildId];
      this.attachChild.link = true;
      this.joint = true;
    }
    if (this.attachParentId) {
      this.attachParent = this.entities[this.attachParentId];
    }
    if (this.geometry) {
      this.geometry.resolve();
    }
    if (this.motion) {
      this.motion.resolve();
    }
    if (this.coordinateSystems) {
      this.coordinateSystems.forEach(c => c.resolve());
    }
    if (this.dataItems) {
      this.dataItems.forEach(c => c.resolve());
    }
  }

  load(complete) {
    if (this.geometry) {
      this.geometry.load(geo => {
        this.root.attach();
        complete(geo);
      });
    }
    this.children.forEach(c => c.load(complete));
  }

  attach() {
    if (this.attachParent && this.attachParent.geometry && this.attachChild && this.attachChild.geometry) {
      // Determine if this is the first axis in a geometric chain. This will occur when the parent does
      // not have motion.
      if (!this.attachParent.link) {  
        this.rootAxis = true;
      }
      
      if (this.motion && this.motion.translation && this.motion.type == 'REVOLUTE' &&
          (this.motion.translation.x != 0 || this.motion.translation.y != 0 || this.motion.translation.z != 0)) {
        const mod = this.attachChild.geometry.model;
        const geo = mod.geometry;
        
        const v = new THREE.Vector3(this.motion.translation.x / 1000.0,
                                    this.motion.translation.y / 1000.0,
                                    this.motion.translation.z / 1000.0);
        const vt = new THREE.Vector3().copy(v).negate();
        geo.translate(vt.x, vt.y, vt.z);
        mod.translateX(v.x).translateY(v.y).translateZ(v.z);
      }
      
      const pg = this.attachParent.geometry.model;
      if (this.rootAxis && this.motion.coordinateSystem)
      {
        // const shape = this.motion.coordinateSystem.shape();
        // pg.attach(shape);
      }
      pg.attach(this.attachChild.geometry.model);
    }
    this.children.forEach(c => c.attach());
  }

  findCoordinateSystem(type) {
    const systems = []
    if (this.coordinateSystems) {
      for (const c of this.coordinateSystems) {
        if (c.type == type)
          systems.push(c)
      }
    }

    systems.concat(this.children.map(c => c.findCoordinateSystem(type)));
    return systems.flat();
  }

  findDataItems(pred) {
    const items = [];

    if (this.dataItems) {
      for (const di of this.dataItems) {
        if (pred(di)) {
          items.push(di);
        }
      }
    } 

    return items.concat(this.children.map(c => c.findDataItems(pred)).flat())
  }

  findComponents(type, name = undefined) {
    const comps = [];

    if (this.type == type && (!name || name == this.name)) {
      comps.push(this);
    } 

    return comps.concat(this.children.map(c => c.findComponents(type, name)).flat())
  }

  findComponent(type, name = undefined) {
    if (this.type == type && (!name || name == this.name)) {
      return this;
    } 

    for (const comp of this.children) {
      const r = comp.findComponent(type, name);
      if (r) return r;
    }    

    return null;
  }

  findGeometries(root = false) {
    const geometries = [];
    if (this.geometry && (!root || !this.geometry.parent)) {
      geometries.push(this.geometry);
    }
    geometries.concat(this.children.map(c => c.findGeometries(root)));
    return geometries.flat();
  }

  findGeometry(name) {
    if (this.geometry && this.name == name) {
      return this.geometry;
    }
    for (const comp of this.children) {
      const g = comp.findGeometry(name);
      if (g) return g;
    }

    return null;
  }
}

class Device extends Component {
  constructor(doc, parser) {
    super('Device', doc, parser);
    this.resolve();    
  }
}

export { Device };
// exports.Axis = Axis;
// exports.Linear = Linear;
// exports.Rotary = Rotary;
// exports.Link = Link;

