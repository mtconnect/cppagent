import * as JP from "../jsonpath/jsonpath.js";

class ProtocolError extends Error {  
}

class JsonParserV1 {
  constructor() {
  }

  devices(probe, fun) {
    const devs = JSONPath.JSONPath({
      path: "$..Devices[0].Device",
      json: probe
    });
    
    return devs.map(fun);
  }

  components(doc, fun) {
    const components = JSONPath.JSONPath({
      path: '$.Components.*',
      json: doc
    }).map(c => {
      const t = Object.keys(c)[0];
      return fun(t, c[t]);
    });

    return components;
  }

  dataItems(doc, fun) {
    const obj = doc.DataItems;
    if (obj) {
      const items = obj.map(v => {
        return fun(v.DataItem);
      });
      return items;
    } else {
      return null;
    }  
  }

  componentRelationships(doc, fun) {
    return doc.filter(r => r.ComponentRelationship).map(cr => {
      return fun(cr.ComponentRelationship);
    });
  }

  coordinateSystems(doc, fun) {
    return doc.map(cs => {
      return fun(cs.CoordinateSystem);
    });
  }

  observations(data, fun) {
    const values = JSONPath.JSONPath({ path: '$..ComponentStream.[Events,Samples,Condition].*', json: data });
    values.forEach( v => {
      let [key, data] = Object.entries(v)[0];
      fun(key, data);
    });
  }
}

export { JsonParserV1 };
