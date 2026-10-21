"""Export replay data through the official RenderDoc API; Python 3.6 compatible."""
import json
import math
from pathlib import Path

try:
    from .renderdoc_capture import fingerprint
except ImportError:
    from renderdoc_capture import fingerprint


def action_stage_scope(action):
    """Describe inspected stages for the selected action, not retained bindings."""
    if action.get('mesh_draw'):
        kind, candidates, uninspected = 'mesh_draw', ['Pixel'], ['Task', 'Mesh']
    elif action.get('ray_dispatch'):
        kind, candidates, uninspected = 'ray_dispatch', [], [
            'RayGen', 'Intersection', 'AnyHit', 'ClosestHit', 'Miss', 'Callable']
    elif action.get('dispatch'):
        kind, candidates, uninspected = 'dispatch', ['Compute'], []
    elif action.get('draw'):
        kind, candidates, uninspected = 'draw', ['Vertex', 'Pixel'], ['Hull', 'Domain', 'Geometry']
    else:
        kind, candidates, uninspected = 'other', [], []
    return {'kind': kind, 'export_candidates': candidates,
        'uninspected_stages': uninspected,
        'scope': 'reflected shaders of the action pipeline only; stage coverage is explicit'}


def active_stage_reflections(rd, pipeline, action):
    """Never query compute for a draw or graphics for a compute dispatch."""
    for name in action_stage_scope(action)['export_candidates']:
        stage = getattr(rd.ShaderStage, name)
        reflection = pipeline.GetShaderReflection(stage)
        if reflection:
            yield stage, reflection


def _variable(variable):
    count = variable.rows * variable.columns
    floats = [value if math.isfinite(value) else str(value)
              for value in list(variable.value.f32v)[:count]]
    return {'name': variable.name, 'type': str(variable.type),
        'rows': variable.rows, 'columns': variable.columns,
        'float32': floats, 'uint32': list(variable.value.u32v)[:count],
        'members': [_variable(member) for member in variable.members]}


def _descriptor(used):
    bound = used.descriptor
    return {'resource': str(bound.resource), 'view': str(bound.view),
        'format': bound.format.Name(), 'byte_offset': bound.byteOffset,
        'byte_size': bound.byteSize, 'first_mip': bound.firstMip, 'mip_count': bound.numMips,
        'first_slice': bound.firstSlice, 'slice_count': bound.numSlices,
        'shader_binding_index': used.access.index, 'array_element': used.access.arrayElement}


def replay_capture(rd, request, receipt):
    output = Path(request['output'])
    capture = rd.OpenCaptureFile()
    controller = None
    def write_json(name, data):
        path = output/name
        path.write_text(json.dumps(data, indent=2), encoding='utf8')
        receipt['artifacts'].append(fingerprint(path))
    try:
        result = capture.OpenFile(request['source']['path'], '', None)
        if result != rd.ResultCode.Succeeded:
            raise RuntimeError('OpenFile: ' + str(result))
        receipt['local_replay_support'] = str(capture.LocalReplaySupport())
        result, controller = capture.OpenCapture(rd.ReplayOptions(), None)
        if result != rd.ResultCode.Succeeded or not controller:
            raise RuntimeError('OpenCapture: ' + str(result))
        properties = controller.GetAPIProperties()
        receipt['api'] = str(properties.pipelineType)
        receipt['degraded'] = properties.degraded
        structured = controller.GetStructuredFile()
        actions = []
        def collect(nodes, parent=None):
            for action in nodes:
                actions.append({'event_id': action.eventId, 'parent_event': parent,
                    'name': action.GetName(structured), 'flags': str(action.flags),
                    'draw': bool(action.flags & rd.ActionFlags.Drawcall),
                    'dispatch': bool(action.flags & rd.ActionFlags.Dispatch),
                    'mesh_draw': bool(action.flags & rd.ActionFlags.MeshDispatch),
                    'ray_dispatch': bool(action.flags & rd.ActionFlags.DispatchRay),
                    'indices': action.numIndices, 'instances': action.numInstances,
                    'outputs': [str(target) for target in action.outputs],
                    'depth_output': str(action.depthOut)})
                collect(action.children, action.eventId)
        collect(controller.GetRootActions())
        if not actions:
            raise RuntimeError('Capture contains no actions')
        draws = [action for action in actions if action['draw'] or action['mesh_draw']]
        receipt['action_count'] = len(actions)
        receipt['draw_count'] = len(draws)
        receipt['dispatch_count'] = sum(action['dispatch'] for action in actions)
        write_json('actions.json', actions)
        names = {str(resource.resourceId): resource.name for resource in controller.GetResources()}
        textures = [{'resource': str(texture.resourceId), 'name': names.get(str(texture.resourceId), ''),
            'width': texture.width, 'height': texture.height, 'depth': texture.depth,
            'mips': texture.mips, 'arraysize': texture.arraysize,
            'format': texture.format.Name(), 'bytes': texture.byteSize}
            for texture in controller.GetTextures()]
        buffers = [{'resource': str(buffer.resourceId), 'name': names.get(str(buffer.resourceId), ''),
                    'bytes': buffer.length} for buffer in controller.GetBuffers()]
        receipt['texture_count'] = len(textures)
        receipt['buffer_count'] = len(buffers)
        write_json('resources.json', {'textures': textures, 'buffers': buffers})
        events = request['events']
        if not events and draws:
            events = sorted({draws[0]['event_id'], draws[-1]['event_id']})
        known = {action['event_id']: action for action in actions}
        pipelines = []
        for event in events:
            if event not in known:
                raise ValueError('Unknown event {}'.format(event))
            controller.SetFrameEvent(event, True)
            pipeline = controller.GetPipelineState()
            action = known[event]
            scope = action_stage_scope(action)
            is_graphics = scope['kind'] in ('draw', 'mesh_draw')
            viewport = pipeline.GetViewport(0)
            state = {'event_id': event, 'action': action, 'stage_coverage': scope,
                'topology': str(pipeline.GetPrimitiveTopology()) if is_graphics else None,
                'bound_graphics_pipeline': str(pipeline.GetGraphicsPipelineObject()),
                'bound_compute_pipeline': str(pipeline.GetComputePipelineObject()),
                'viewport0': {'x': viewport.x, 'y': viewport.y, 'width': viewport.width,
                    'height': viewport.height, 'min_depth': viewport.minDepth,
                    'max_depth': viewport.maxDepth} if is_graphics else None,
                'stages': []}
            for stage, reflection in active_stage_reflections(rd, pipeline, action):
                shader = pipeline.GetShader(stage)
                entry = pipeline.GetShaderEntryPoint(stage)
                pipeline_object = (pipeline.GetComputePipelineObject() if stage == rd.ShaderStage.Compute
                                   else pipeline.GetGraphicsPipelineObject())
                shader_state = {'stage': str(stage), 'activity': 'active_action_pipeline',
                    'shader': str(shader), 'entry': entry,
                    'encoding': str(reflection.encoding), 'constant_blocks': [],
                    'declared_resources': [{'name': resource.name, 'binding': resource.fixedBindNumber,
                        'set_or_space': resource.fixedBindSetOrSpace, 'texture': resource.isTexture}
                        for resource in reflection.readOnlyResources],
                    'read_only_bindings': [_descriptor(binding) for binding in pipeline.GetReadOnlyResources(stage)],
                    'read_write_bindings': [_descriptor(binding) for binding in pipeline.GetReadWriteResources(stage)]}
                shader_path = output/('event-{}-{}.shader'.format(event, str(stage)))
                shader_path.write_bytes(bytes(reflection.rawBytes))
                receipt['artifacts'].append(fingerprint(shader_path))
                shader_state['binary'] = fingerprint(shader_path)
                disassembly = controller.DisassembleShader(pipeline_object, reflection, '')
                disassembly_path = output/('event-{}-{}.txt'.format(event, str(stage)))
                disassembly_path.write_text(disassembly, encoding='utf8')
                receipt['artifacts'].append(fingerprint(disassembly_path))
                for index, block in enumerate(reflection.constantBlocks):
                    bound = pipeline.GetConstantBlock(stage, index, 0).descriptor
                    variables = controller.GetCBufferVariableContents(pipeline_object,
                        shader, stage, entry, index, bound.resource, bound.byteOffset, bound.byteSize)
                    shader_state['constant_blocks'].append({'name': block.name,
                        'buffer': str(bound.resource), 'offset': bound.byteOffset, 'length': bound.byteSize,
                        'variables': [_variable(variable) for variable in variables]})
                state['stages'].append(shader_state)
            targets = pipeline.GetOutputTargets() if is_graphics else []
            state['output_targets'] = [str(target.resource) for target in targets]
            state['depth_target'] = str(pipeline.GetDepthTarget().resource) if is_graphics else None
            if targets and targets[0].resource != rd.ResourceId.Null():
                save = rd.TextureSave()
                save.resourceId = targets[0].resource
                save.destType = rd.FileType.PNG
                path = output/('event-{}-target0.png'.format(event))
                result = controller.SaveTexture(save, str(path))
                if result != rd.ResultCode.Succeeded:
                    raise RuntimeError('SaveTexture: ' + str(result))
                receipt['artifacts'].append(fingerprint(path))
            pipelines.append(state)
        write_json('pipelines.json', pipelines)
        debug_messages = [{'event_id': message.eventId, 'severity': str(message.severity),
            'category': str(message.category), 'description': message.description}
            for message in controller.GetDebugMessages()]
        write_json('debug-messages.json', debug_messages)
        receipt['debug_message_count'] = len(debug_messages)
        receipt['selected_events'] = events
        receipt['limitations'] = ['PNG exports are display images; inspect native formats and shader data for numeric evidence',
            'Bound pipeline objects may be retained; exported shaders belong only to the selected action pipeline',
            'Stage exports cover Vertex, Pixel and Compute; uninspected stages are listed per event',
            'Application shutdown and Vulkan validation-layer activation require separate host evidence']
        receipt['status'] = 'replayed'
    finally:
        if controller:
            controller.Shutdown()
        capture.Shutdown()
