import os
import numpy as np
import torch
import torchvision.models as models
import torchvision.transforms as transforms
from torch.utils.dlpack import from_dlpack
from PIL import Image

def load_model():
    """ Load a ResNet model for inference """
    from torchvision.models import ResNet18_Weights
    model = models.resnet18(weights=ResNet18_Weights.DEFAULT)
    model.eval()
    model.to("cuda")  # Move model to GPU
    return model

def preprocess(gpu_ptr, size):
    """
    Convert raw buffer data into a 4D tensor (N, C, H, W) for Conv2D.
    - `gpu_ptr`: Raw memory buffer from C
    - `size`: Number of elements
    """
    dtype = torch.float32
    image_size = (3, 224, 224)  # Example input shape for ResNet
    batch_size = 1

    # Convert bytes buffer to numpy array
    np_array = np.frombuffer(gpu_ptr, dtype=np.float32, count=size)

    # Reshape to (N, C, H, W) for Conv2D
    tensor = torch.tensor(np_array, dtype=dtype, device="cuda").reshape((batch_size, *image_size))

    return tensor

def infer(model, input_tensor):
    """ Perform inference with the given model and input tensor directly in GPU """
    with torch.no_grad():
        output = model(input_tensor)
    return output  # Keep result in GPU memory for DMA transfer

def postprocess(output_tensor, top_k=5):
    """ Convert model output into human-readable results """
    probabilities = torch.nn.functional.softmax(output_tensor[0], dim=0)
    top_probs, top_labels = torch.topk(probabilities, top_k)
    
    # Load class labels
    script_dir = os.path.dirname(os.path.abspath(__file__))
    classes_path = os.path.join(script_dir, "data/imagenet_classes.txt")

    with open(classes_path) as f:
        class_names = [line.strip() for line in f.readlines()]
    
    results = [(class_names[label.item()], prob.item()) for label, prob in zip(top_labels, top_probs)]
    return results

if __name__ == "__main__":
    # Example usage (this won't run in C; just for testing in Python)
    model = load_model()
    # Fake input for testing
    input_tensor = torch.randn(1, 3, 224, 224, device="cuda")
    output_tensor = infer(model, input_tensor)
    results = postprocess(output_tensor)

    for label, prob in results:
        print(f"{label}: {prob:.4f}")

