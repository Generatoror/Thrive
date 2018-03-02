#!/usr/bin/env ruby
# Generates Thrive world for the cell stage

require_relative '../../RubySetupSystem/RubyCommon.rb'
require_relative '../../ThirdParty/leviathan/Helpers/FileGen.rb'

abort "no target files provided" if ARGV.count < 2

generator = Generator.new ARGV[0], separateFiles: true

generator.useNamespace "thrive"
# generator.useExportMacro "THRIVE_EXPORT"
generator.useExportMacro nil
generator.addInclude "Entities/GameWorld.h"
generator.addInclude "Generated/StandardWorld.h"
# Needs script include for basic world functionality
generator.addInclude "Script/ScriptTypeResolver.h"

generator.addInclude "microbe_stage/membrane_system.h"
generator.addInclude "microbe_stage/compound_cloud_system.h"
generator.addInclude "microbe_stage/process_system.h"
generator.addInclude "microbe_stage/species_component.h"
generator.addInclude "microbe_stage/spawn_system.h"
generator.addInclude "microbe_stage/agent_cloud_system.h"
generator.addInclude "microbe_stage/compound_absorber_system.h"
generator.addInclude "microbe_stage/microbe_camera_system.h"

world = GameWorldClass.new(
  "CellStageWorld", componentTypes: [
    EntityComponent.new("ProcessorComponent", [ConstructorInfo.new([])]),
    EntityComponent.new("CompoundBagComponent", [ConstructorInfo.new([])]),
    EntityComponent.new("SpeciesComponent", [ConstructorInfo.new([
                                                Variable.new("name", "std::string",
                                                            noRef: false)
                                            ])]),
    EntityComponent.new("MembraneComponent", [ConstructorInfo.new(
                                         [
                                           #Variable.new("GetScene()", "",
                                           #             nonMethodParam: true),
                                         ])], releaseparams: ["GetScene()"]),
    EntityComponent.new("CompoundCloudComponent", [ConstructorInfo.new(
                                                     [
                                                       Variable.new("compoundId", "CompoundId",
                                                                    noRef: true),
                                                       Variable.new("red", "float",
                                                                    noRef: true),
                                                       Variable.new("green", "float",
                                                                    noRef: true),
                                                       Variable.new("blue", "float",
                                                                    noRef: true),
                                                     ])]),
    EntityComponent.new("AgentCloudComponent", [ConstructorInfo.new(
                                                  [
                                                    Variable.new("compoundId", "CompoundId",
                                                                 noRef: true),
                                                    Variable.new("red", "float",
                                                                 noRef: true),
                                                    Variable.new("green", "float",
                                                                 noRef: true),
                                                    Variable.new("blue", "float",
                                                                 noRef: true),
                                                  ])]),
    EntityComponent.new("SpawnedComponent", [ConstructorInfo.new(
                                               [
                                                 Variable.new("newSpawnRadius", "double",
                                                              noRef: true)
                                               ])]),
    EntityComponent.new("CompoundAbsorberComponent", [ConstructorInfo.new([])]),
    
  ],
  systems: [
    EntitySystem.new("MembraneSystem", ["MembraneComponent", "RenderNode"],
                     # This is ran only once and the animation is in
                     # the vertex shader. That's why this isn't in
                     # "runrender"
                     runtick: {group: 100, parameters: [
                                   "GetScene()"
                                 ]}),

    EntitySystem.new("SpawnSystem", [],
                     runtick: {group: 50, parameters: []},
                     visibletoscripts: true),

    EntitySystem.new("AgentCloudSystem", ["Position", "AgentCloudComponent", "RenderNode"],
                     runtick: {group: 5, parameters: []}),

    EntitySystem.new("CompoundAbsorberSystem", ["AgentCloudComponent", "Position",
                                                "MembraneComponent",
                                                "CompoundAbsorberComponent"],
                     runtick: {group: 6, parameters: [
                                 "ComponentCompoundCloudComponent.GetIndex()"]}),

    EntitySystem.new("MicrobeCameraSystem", [],
                     runtick: {group: 1000, parameters: []}),

    EntitySystem.new("ProcessSystem", ["CompoundBagComponent", "ProcessorComponent"],
                     runtick: {group: 10, parameters: []}),

    #EntitySystem.new("ProcessSystem", ["CompoundBagComponent", "ProcessorComponent"],

    # EntitySystem.new("CompoundCloudSystem", [],
    #                  nostate: true,
    #                  init: [Variable.new("*this", "")],
    #                  release: [Variable.new("*this", "")],
    #                  runtick: {group: 5, parameters: [
    #                              "ComponentCompoundCloudComponent.GetIndex()",
    #                              "GetTickNumber()"
    #                            ]})
  ],
  systemspreticksetup: (<<-END
  const auto timeAndTickTuple = GetTickAndTime();
  const auto calculatedTick = std::get<0>(timeAndTickTuple);
  const auto progressInTick = std::get<1>(timeAndTickTuple);
  const auto tick = GetTickNumber();
END
                       ),  
)

world.base "Leviathan::StandardWorld"

generator.add world



# Output the file
generator.run


bindGenerator = Generator.new ARGV[1], bareOutput: true


bindGenerator.add OutputText.new(world.genAngelScriptBindings)


bindGenerator.run
