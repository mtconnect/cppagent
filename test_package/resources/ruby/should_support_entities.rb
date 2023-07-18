
puts "Creating a simple entity"

$ent1 = MTConnect::Entity.new("TestEntity", "Simple Value")
raise "Entity is null" unless $ent1
raise "Bad Value" unless $ent1.value == "Simple Value"

puts "Creating an entity with multiple properties"

$ent2 = MTConnect::Entity.new("HashEntity", VALUE: "Simple Value", int: 10, float: 123.4,
                              time: Time.gm(2020, 1, 1) )

