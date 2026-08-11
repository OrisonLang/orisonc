#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace orison::pipeline {

struct ComputedDynamicArrayForDescriptorRenderState {
    std::vector<std::string> enclosing_function_names;
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> source_type_names;
    std::vector<std::string> element_source_type_names;
    std::vector<std::string> descriptor_storage_names;
    std::vector<std::string> descriptor_value_names;
    std::vector<std::string> data_pointer_names;
    std::vector<std::string> length_names;
    std::vector<std::string> capacity_names;
    bool render_metadata_available = false;
    bool all_descriptor_projections_ready = false;
    std::size_t render_count = 0;
    std::size_t rendered_ir_snippet_count = 0;
};

struct ComputedDynamicArrayForLoopControlRenderState {
    std::vector<std::string> enclosing_function_names;
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> source_type_names;
    std::vector<std::string> element_source_type_names;
    std::vector<std::string> condition_block_names;
    std::vector<std::string> body_block_names;
    std::vector<std::string> continue_block_names;
    std::vector<std::string> exit_block_names;
    std::vector<std::string> index_names;
    std::vector<std::string> next_index_names;
    std::vector<std::string> bounds_check_names;
    bool render_metadata_available = false;
    bool all_control_flow_names_ready = false;
    std::size_t render_count = 0;
    std::size_t rendered_ir_snippet_count = 0;
};

struct ComputedDynamicArrayForElementAddressRenderState {
    std::vector<std::string> enclosing_function_names;
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> source_type_names;
    std::vector<std::string> element_source_type_names;
    std::vector<std::string> element_llvm_type_names;
    std::vector<std::string> data_pointer_names;
    std::vector<std::string> index_names;
    std::vector<std::string> element_address_names;
    bool render_metadata_available = false;
    bool all_element_address_inputs_ready = false;
    std::size_t render_count = 0;
    std::size_t rendered_ir_snippet_count = 0;
};

struct ComputedDynamicArrayForElementLoadRenderState {
    std::vector<std::string> enclosing_function_names;
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> source_type_names;
    std::vector<std::string> element_source_type_names;
    std::vector<std::string> element_llvm_type_names;
    std::vector<std::string> element_address_names;
    std::vector<std::string> item_value_names;
    bool render_metadata_available = false;
    bool all_element_load_inputs_ready = false;
    std::size_t render_count = 0;
    std::size_t rendered_ir_snippet_count = 0;
};

struct ComputedDynamicArrayForLoopContinueRenderState {
    std::vector<std::string> enclosing_function_names;
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> source_type_names;
    std::vector<std::string> element_source_type_names;
    std::vector<std::string> continue_block_names;
    std::vector<std::string> condition_block_names;
    std::vector<std::string> index_names;
    std::vector<std::string> next_index_names;
    bool render_metadata_available = false;
    bool all_loop_continue_inputs_ready = false;
    std::size_t render_count = 0;
    std::size_t rendered_ir_snippet_count = 0;
};

struct ComputedDynamicArrayForLoopRenderSequenceState {
    std::vector<std::string> enclosing_function_names;
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> source_type_names;
    std::vector<std::string> element_source_type_names;
    std::vector<std::string> body_block_names;
    bool sequence_metadata_available = false;
    bool all_body_blocks_ready = false;
    std::size_t sequence_count = 0;
    std::size_t rendered_ir_snippet_count = 0;
};

struct ComputedDynamicArrayForLoopExitCleanupState {
    std::vector<std::string> enclosing_function_names;
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> source_type_names;
    std::vector<std::string> element_source_type_names;
    std::vector<std::string> exit_block_names;
    std::vector<std::string> loop_entry_cleanup_owner_names;
    std::vector<std::string> loop_exit_cleanup_owner_names;
    std::vector<std::string> cleanup_resumption_operation_names;
    bool cleanup_metadata_available = false;
    bool all_cleanup_resumptions_ready = false;
    std::size_t cleanup_count = 0;
    std::size_t rendered_ir_snippet_count = 0;
};

struct ComputedDynamicArrayForCleanupTransitionState {
    std::vector<std::string> enclosing_function_names;
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> source_type_names;
    std::vector<std::string> element_source_type_names;
    std::vector<std::string> acquisition_source_owner_names;
    std::vector<std::string> acquisition_target_owner_names;
    std::vector<std::string> acquisition_operation_names;
    std::vector<std::string> resumption_source_owner_names;
    std::vector<std::string> resumption_target_owner_names;
    std::vector<std::string> resumption_operation_names;
    bool transition_metadata_available = false;
    bool all_transitions_paired = false;
    std::size_t transition_count = 0;
};

} // namespace orison::pipeline
