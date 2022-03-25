
import * as JP from "../jsonpath/jsonpath.js";
import * as THREE from '../threejs/three.module.js';
import { OBJLoader } from '../threejs/jsm/loaders/OBJLoader.js';
import { STLLoader } from '../threejs/jsm/loaders/STLLoader.js';

const _radians = new THREE.Vector3(1.0 / 180.0 * Math.PI, 1.0 / 180.0 * Math.PI, 1.0 / 180.0 * Math.PI);
const _meters = new THREE.Vector3(0.001, 0.001, 0.001);

class CoordinateSystem {
  constructor(doc, comp) {
    Object.assign(this, doc);
    this.component = comp;
    comp.entities[this.id] = this;
  }

  translate(vector) {

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
    if (this.Scale) {
      this.scale.fromArray(this.Scale);
      this.scale.multiply(_meters);
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
    }
  }

  position(pos) {
    const p = pos.multiply(_meters);
    //console.log("position", p);
    this.model.position.copy(p);
  }

  rotate(rot) {
    const v = _radians.clone().multiply(rot);

    //console.log("rotate", v);
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
  }

  resolve() {
    if (this.coordinateSystemIdRef) {
      this.coordinateSystem = this.component.entities[this.coordinateSystemIdRef];
    }
    if (this.component.motion) {
      this.motion = this.component.motion.type;
      this.axis = this.component.motion.axis;
    }
  }

  apply(obs) {
    const [key, data] = Object.entries(obs)[0]; 

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
              let [k, v] = Object.entries(o)[0]; 
              return v.nativeCode == data.nativeCode;
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
            this.value.push(obs);
          }
        }
      }
    } else {
      if (data.value == 'UNAVAILABLE') {
        obs[key].value = null;
      }

      this.value = obs;
      const value = obs[key].value;

      if (((this.type == 'POSITION' || this.type == 'ANGLE') && this.subType == 'ACTUAL') &&
          typeof value == 'number') {
        const child = this.component.attachChild;
        if (child && child.geometry && this.axis) {
          const vec = this.axis.clone().multiplyScalar(value);
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
  constructor(type, doc, parent = null) {
    Object.assign(this, doc);
    this.type = type;
    this.parent = parent;
    this.parse(doc);

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

  parse(doc) {
    this.children = JSONPath.JSONPath({
      path: '$.Components.*',
      json: doc
    }).map(c => {
      const t = Object.keys(c)[0];
      return new Component(t, c[t], this);
    });

    if (this.Configuration) {
      const config = this.Configuration;
      if (config.SolidModel) {
        this.geometry = new Geometry(config.SolidModel, this);
      } 
      
      if (config.Relationships) {
        for (const rel of config.Relationships) {
          if (rel.ComponentRelationship) {
            const type = rel.ComponentRelationship.type;
            if (type == 'PARENT') {
              this.attachParentId = rel.ComponentRelationship.idRef;
            } else if (type == 'CHILD') {
              this.attachChildId = rel.ComponentRelationship.idRef;
            }
          }
        }
      }
      
      if (config.Motion) { 
        this.motion = new Motion(config.Motion, this);
      }
       
      if (config.CoordinateSystems) {
        this.coordinateSystems = config.CoordinateSystems.map(c => 
          new CoordinateSystem(c.CoordinateSystem, this));
      }
    }
    if (this.DataItems) {
      this.dataItems = this.DataItems.map(c => 
        new DataItem(c.DataItem, this));
    }
  }

  resolve() {
    for (const c of this.children) {
      c.resolve();
    }
    if (this.attachChildId) {
      this.attachChild = this.entities[this.attachChildId]
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
      const pg = this.attachParent.geometry.model;
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
  constructor(doc) {
    super('Device', doc);
    this.resolve();    
  }
}

export { Device };
// exports.Axis = Axis;
// exports.Linear = Linear;
// exports.Rotary = Rotary;
// exports.Link = Link;

