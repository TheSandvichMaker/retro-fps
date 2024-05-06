#pragma once

fn D3D12_FILL_MODE               to_d3d12_fill_mode              (rhi_fill_mode_t mode);
fn D3D12_CULL_MODE               to_d3d12_cull_mode              (rhi_cull_mode_t mode);
fn D3D12_COMPARISON_FUNC         to_d3d12_comparison_func        (rhi_comparison_func_t func);
fn D3D12_BLEND                   to_d3d12_blend                  (rhi_blend_t blend);
fn D3D12_BLEND_OP                to_d3d12_blend_op               (rhi_blend_op_t op);
fn D3D12_LOGIC_OP                to_d3d12_logic_op               (rhi_logic_op_t op);
fn D3D12_PRIMITIVE_TOPOLOGY_TYPE to_d3d12_primitive_topology_type(rhi_primitive_topology_type_t topology);
fn D3D12_STENCIL_OP              to_d3d12_stencil_op             (rhi_stencil_op_t op);
fn DXGI_FORMAT                   to_dxgi_format                  (rhi_pixel_format_t pf);
