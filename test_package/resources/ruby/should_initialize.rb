
$pipelines = []

MTConnect.agent.sources.each do |s|
  $pipelines << s.pipeline
end

