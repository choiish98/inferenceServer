# inference.py
import torch
import torchvision.models as models

def load_model():
    """Load a pretrained ResNet18 model and set it to evaluation mode."""
    model = models.resnet18(pretrained=True)  # Load ResNet18 model
    model.eval()  # Set model to evaluation mode
    return model

def infer(model, input_tensor):
    """Perform inference on the input tensor using the loaded model.

    Args:
        model: PyTorch model in evaluation mode.
        input_tensor: Input tensor for inference.

    Returns:
        output: Raw output tensor from the model.
    """
    with torch.no_grad():  # Disable gradient computation
        output = model(input_tensor)  # Perform inference
    return output

