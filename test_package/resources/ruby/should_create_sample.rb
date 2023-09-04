MTConnect::Logger.info "Declaring class FixExecution"

$trans =  MTConnect::RubyTransform.new("CreateSample", :Tokens) do |tokens|
  dev = MTConnect.agent.default_device
  name, value = tokens.tokens
  
  di = dev.data_item(name)
  
  obs = MTConnect::Observation.new(di, value)

  forward(obs)
end
