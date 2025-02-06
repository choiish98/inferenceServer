import sys
import os
import torch
import torchvision.models as models
import torchvision.transforms as transforms
import torch.nn.functional as F
from PIL import Image

def load_model(model_name="resnet18"):
    """ Load a pre-trained model """
    model_name = model_name.lower()
    
    if model_name == "resnet18":
        model = load_resnet18()
    elif model_name == "resnet50":
        model = load_resnet50()
    else:
        raise ValueError(f"Unsupported model: {model_name}")

    model.eval()
    model.to("cuda")  # Move model to GPU
    return model

def load_resnet18():
    """ Load a ResNet model for inference """
    from torchvision.models import ResNet18_Weights
    model = models.resnet18(weights=ResNet18_Weights.DEFAULT)
    return model

def load_resnet50():
    """ Load a ResNet model for inference """
    from torchvision.models import ResNet50_Weights
    model = models.resnet50(weights=ResNet50_weights.DEFAULT)
    return model

def preprocess(image_path):
    """" Load an image, apply preprocessing transformations. """
    transform = transforms.Compose([
        transforms.Resize((224, 224)),
        transforms.ToTensor(),
        transforms.Normalize(mean=[0.485, 0.456, 0.406], 
            std=[0.229, 0.224, 0.225])
    ])
    
    image = Image.open(image_path).convert("RGB")
    input_tensor = transform(image).unsqueeze(0).to("cuda")  # GPU로 데이터 이동
    
    return input_tensor

def infer(model, input_tensor):
    """ Perform inference with the given model and input tensor """
    with torch.no_grad():
        output = model(input_tensor)
    return output

def postprocess(output_tensor, top_k=5):
    """ Convert model output into human-readable results """
    probabilities = F.softmax(output_tensor, dim=1).cpu().numpy()
    top_k_indices = probabilities.argsort()[0][-top_k:][::-1]
    top_k_probs = probabilities[0][top_k_indices]
    
    # Load class labels
    script_dir = os.path.dirname(os.path.abspath(__file__))
    classes_path = os.path.join(script_dir, "data/imagenet_classes.txt")

    with open(classes_path) as f:
        labels = [line.strip() for line in f.readlines()]
    
    results = [(labels[i], top_k_probs[idx]) for idx, i in enumerate(top_k_indices)]
    return results

if __name__ == "__main__":
    # Example usage (this won't run in C; just for testing in Python)
    model = load_model()
    input_tensor = preprocess("data/example.jpg")
    output_tensor = infer(model, input_tensor)
    results = postprocess(output_tensor)

    for label, prob in results:
        print(f"{label}: {prob:.4f}")

