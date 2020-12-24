// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include "adapter.hpp"
#include "agent.hpp"
#include "xml_printer_helper.hpp"
#include "entity.hpp"
#include "entity/xml_parser.hpp"
#include "entity/xml_printer.hpp"

#include <libxml/xmlwriter.h>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

using namespace std;
using namespace mtconnect;
using namespace mtconnect::entity;

class EntityPrinterTest : public testing::Test
{
 protected:
  void SetUp() override
  {
    m_writer = make_unique<XmlWriter>(true);
  }

  void TearDown() override
  {
    m_writer.reset();
  }
  
  FactoryPtr createFileArchetypeFactory()
  {
    auto fileProperty = make_shared<Factory>(Requirements({
      Requirement("name", true ),
      Requirement("VALUE", true) }));
    
    auto fileProperties = make_shared<Factory>(  Requirements({
      Requirement("FileProperty", ENTITY, fileProperty,
                  1, Requirement::Infinite) }));
    fileProperties->registerMatchers();
    
    auto fileComment = make_shared<Factory>(Requirements({
      Requirement("timestamp", true ),
      Requirement("VALUE", true) }));
    
    auto fileComments = make_shared<Factory>(Requirements({
      Requirement("FileComment", ENTITY, fileComment,
                  1, Requirement::Infinite) }));
    fileComments->registerMatchers();
    
    auto fileArchetype = make_shared<Factory>(Requirements{
      Requirement("assetId", true ),
      Requirement("deviceUuid", true ),
      Requirement("timestamp", true ),
      Requirement("removed", false ),
      Requirement("name", true ),
      Requirement("mediaType", true),
      Requirement("applicationCategory", true),
      Requirement("applicationType", true),
      Requirement("Description", false),
      Requirement("FileComments", ENTITY_LIST, fileComments, false),
      Requirement("FileProperties", ENTITY_LIST, fileProperties, false)
    });
    
    auto root = make_shared<Factory>(Requirements{
      Requirement("FileArchetype", ENTITY, fileArchetype)
    });
    
    return root;
  }

  std::unique_ptr<XmlWriter> m_writer;
};

TEST_F(EntityPrinterTest, TestParseSimpleDocument)
{
  auto root = createFileArchetypeFactory();
  
  auto doc = string {
"<FileArchetype applicationCategory=\"ASSEMBLY\" applicationType=\"DATA\" assetId=\"uuid\" deviceUuid=\"duid\" mediaType=\"json\" name=\"xxxx\" timestamp=\"2020-12-01T10:00Z\">\n"
"  <FileProperties>\n"
"    <FileProperty name=\"one\">Round</FileProperty>\n"
"    <FileProperty name=\"two\">Flat</FileProperty>\n"
"  </FileProperties>\n"
"</FileArchetype>\n"
  };
  
  ErrorList errors;
  entity::XmlParser parser;
  
  auto entity = parser.parse(root, doc, "1.7", errors);
  ASSERT_EQ(0, errors.size());

  entity::XmlPrinter printer;
  
  printer.print(*m_writer, entity);
  ASSERT_EQ(doc, m_writer->getContent());
}

TEST_F(EntityPrinterTest, TestFileArchetypeWithDescription)
{
  auto root = createFileArchetypeFactory();
  
  auto doc = string {
    "<FileArchetype applicationCategory=\"ASSEMBLY\" applicationType=\"DATA\" assetId=\"uuid\" deviceUuid=\"duid\" mediaType=\"json\" name=\"xxxx\" timestamp=\"2020-12-01T10:00Z\">\n"
    "  <Description>Hello there Shaurabh</Description>\n"
    "  <FileProperties>\n"
    "    <FileProperty name=\"one\">Round</FileProperty>\n"
    "    <FileProperty name=\"two\">Flat</FileProperty>\n"
    "  </FileProperties>\n"
    "</FileArchetype>\n"
  };
  
  ErrorList errors;
  entity::XmlParser parser;
  
  auto entity = parser.parse(root, doc, "1.7", errors);
  ASSERT_EQ(0, errors.size());
  
  entity::XmlPrinter printer;
  
  printer.print(*m_writer, entity);
  ASSERT_EQ(doc, m_writer->getContent());
}

TEST_F(EntityPrinterTest, TestRecursiveEntityLists)
{
  auto component = make_shared<Factory>(Requirements{
    Requirement("id", true ),
    Requirement("name", false ),
    Requirement("uuid", false ),
  });
  
  auto components = make_shared<Factory>(Requirements({
    Requirement("Component", ENTITY, component,
                1, Requirement::Infinite) }));
  components->registerMatchers();
  components->registerFactory(regex(".+"), component);
  
  component->addRequirements({
    Requirement("Components", ENTITY_LIST, components, false)
  });
  
  auto device = make_shared<Factory>(*component);
  device->addRequirements(Requirements{
    Requirement("name", true ),
    Requirement("uuid", true ),
  });
  
  auto root = make_shared<Factory>(Requirements{
    Requirement("Device", ENTITY, device)
  });
  
  auto doc = string {
"<Device id=\"d1\" name=\"foo\" uuid=\"xxx\">\n"
"  <Components>\n"
"    <Systems id=\"s1\">\n"
"      <Components>\n"
"        <Electric id=\"e1\"/>\n"
"        <Heating id=\"h1\"/>\n"
"      </Components>\n"
"    </Systems>\n"
"  </Components>\n"
"</Device>\n"
  };
  
  ErrorList errors;
  entity::XmlParser parser;
  
  auto entity = parser.parse(root, doc, "1.7", errors);
  ASSERT_EQ(0, errors.size());

  entity::XmlPrinter printer;
  
  printer.print(*m_writer, entity);
  ASSERT_EQ(doc, m_writer->getContent());
}

TEST_F(EntityPrinterTest, TestEntityOrder)
{
  auto component = make_shared<Factory>(Requirements{
    Requirement("id", true ),
    Requirement("VALUE", false ),
  });
  
  auto components = make_shared<Factory>(Requirements({
    Requirement("ZFirstValue", ENTITY, component),
    Requirement("HSecondValue", ENTITY, component),
    Requirement("AThirdValue", ENTITY, component),
    Requirement("GFourthValue", ENTITY, component),
    Requirement("Simple", false),
    Requirement("Unordered", false)
  }));
  components->setOrder({ "ZFirstValue", "Simple",
    "HSecondValue", "AThirdValue", "GFourthValue"
  });
  
  auto device = make_shared<Factory>(Requirements({
    Requirement("name", true ),
    Requirement("Components", ENTITY, components),
  }));
  
  auto root = make_shared<Factory>(Requirements{
    Requirement("Device", ENTITY, device)
  });

  auto doc = string {
    "<Device name=\"foo\">\n"
    "  <Components>\n"
    "    <Unordered>Last</Unordered>\n"
    "    <HSecondValue id=\"a\">First</HSecondValue>\n"
    "    <GFourthValue id=\"b\">Second</GFourthValue>\n"
    "    <ZFirstValue id=\"c\">Third</ZFirstValue>\n"
    "    <Simple>Fourth</Simple>\n"
    "    <AThirdValue id=\"d\">Fifth</AThirdValue>\n"
    "  </Components>\n"
    "</Device>\n"
  };
  
  ErrorList errors;
  entity::XmlParser parser;
  
  auto entity = parser.parse(root, doc, "1.7", errors);
  ASSERT_EQ(0, errors.size());
  
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity);
  
  auto expected = string {
    "<Device name=\"foo\">\n"
    "  <Components>\n"
    "    <ZFirstValue id=\"c\">Third</ZFirstValue>\n"
    "    <Simple>Fourth</Simple>\n"
    "    <HSecondValue id=\"a\">First</HSecondValue>\n"
    "    <AThirdValue id=\"d\">Fifth</AThirdValue>\n"
    "    <GFourthValue id=\"b\">Second</GFourthValue>\n"
    "    <Unordered>Last</Unordered>\n"
    "  </Components>\n"
    "</Device>\n"
  };

  ASSERT_EQ(expected, m_writer->getContent());
}

TEST_F(EntityPrinterTest, TestRawContent)
{
  auto definition = make_shared<Factory>(Requirements({
    Requirement("format", false),
    Requirement("RAW", true)
  }));
  
  auto root = make_shared<Factory>(Requirements({
    Requirement("Definition", ENTITY, definition, true)
  }));

  auto doc = R"DOC(
<Definition format="XML">
  <SomeContent with="stuff">
    And some text
  </SomeContent>
  <AndMoreContent/>
  And random text as well.
</Definition>
)DOC";
  
  ErrorList errors;
  entity::XmlParser parser;
  
  auto entity = parser.parse(root, doc, "1.7", errors);
  ASSERT_EQ(0, errors.size());
  
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity);

  auto expected = R"DOC(<Definition format="XML"><SomeContent with="stuff">
    And some text
  </SomeContent><AndMoreContent/>
  And random text as well.
</Definition>
)DOC";
  
  ASSERT_EQ(expected, m_writer->getContent());
}
