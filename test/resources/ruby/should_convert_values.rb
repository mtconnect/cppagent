
puts "Creating module ConvertValuesFromRuby"

module ConvertValuesFromRuby
  def self.create_entity_1
    MTConnect::Entity.new("TestEntity1", { VALUE: 10 })

  rescue
    puts $!
    puts $!.backtrace.join("\n")
    nil
  end

  def self.create_entity_2
    puts "Creating entity 2"
    MTConnect::Entity.new("TestEntity2", { VALUE: "Hello" })
  rescue
    puts $!
    puts $!.backtrace.join("\n")
    nil
  end

  def self.create_entity_3
    puts "Creating entity 3"
    MTConnect::Entity.new("TestEntity3", { VALUE: 123.5 })
  rescue
    puts $!
    puts $!.backtrace.join("\n")
    nil
  end

end



