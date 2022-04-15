MTConnect::Logger.info "Declaring class CreateEvent"

$trans =  MTConnect::RubyTransform.new("CreateEvent") do |event|
  dev = MTConnect.agent.default_device
  data = eval(event.value)
    
  di = dev.data_item(data[:name])
  
  obs = MTConnect::Observation.new(di, data[:value])

  forward(obs)
end
