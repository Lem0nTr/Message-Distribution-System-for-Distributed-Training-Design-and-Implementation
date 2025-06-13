# aggregate_models.py
import argparse
import torch
import sys
from ultralytics import YOLO

def load_model_weights(model_path):
    """直接加载模型参数（跳过数据量检查）"""
    try:
        model = YOLO(model_path)
        return model.model.state_dict()
    except Exception as e:
        raise RuntimeError(f"加载模型失败: {str(e)}")

def fixed_weight_average(params_list):
    """固定权重平均（0.5:0.5）"""
    aggregated_params = {}
    for key in params_list[0].keys():
        # 跳过BN统计量
        if 'running_mean' in key or 'running_var' in key:
            aggregated_params[key] = params_list[0][key].clone()
            continue
        
        # 平均参数
        aggregated_params[key] = (params_list[0][key] + params_list[1][key]) / 2
    return aggregated_params

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="固定权重聚合器")
    parser.add_argument("--model1", type=str, required=True)
    parser.add_argument("--model2", type=str, required=True)
    parser.add_argument("--output", type=str, required=True)
    args = parser.parse_args()

    try:
        # 加载模型参数
        model1_params = load_model_weights(args.model1)
        model2_params = load_model_weights(args.model2)
        
        # 执行聚合
        aggregated_params = fixed_weight_average([model1_params, model2_params])
        
        # 保存模型
        base_model = YOLO(args.model1)
        base_model.model.load_state_dict(aggregated_params)
        base_model.save(args.output)
        
        print(f"聚合成功！输出文件: {args.output}")
    except Exception as e:
        print(f"[错误] {str(e)}", file=sys.stderr)
        sys.exit(1)
