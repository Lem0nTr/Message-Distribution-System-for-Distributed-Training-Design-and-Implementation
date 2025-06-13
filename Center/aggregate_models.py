import argparse
import torch
import sys
from pathlib import Path

def aggregate_models(edge_model_path, global_model_path, output_path):
    """核心聚合逻辑：加权平均边缘模型和全局模型"""
    try:
        # 加载参数
        edge_params = torch.load(edge_model_path)
        global_params = torch.load(global_model_path)
        
        # ==== 参数检查 ====
        assert edge_params.keys() == global_params.keys(), "模型结构不一致"
        for key in edge_params:
            assert edge_params[key].shape == global_params[key].shape, f"层 {key} 形状不匹配"
        
        # ==== 聚合（示例：简单平均）====
        aggregated = {}
        for key in edge_params:
            aggregated[key] = (edge_params[key] + global_params[key]) / 2
        
        # 保存结果
        Path(output_path).parent.mkdir(parents=True, exist_ok=True)
        torch.save(aggregated, output_path)
        print(f"聚合成功！模型保存至 {output_path}")
        return True
    except Exception as e:
        print(f"聚合失败: {str(e)}", file=sys.stderr)
        return False

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--edge-model", required=True)
    parser.add_argument("--global-model", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--round", type=int)
    args = parser.parse_args()
    
    if not aggregate_models(args.edge_model, args.global_model, args.output):
        sys.exit(1)
