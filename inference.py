# inference.py
import torch
import torchvision.models as models
import torchvision.transforms as transforms
from PIL import Image
import torch.nn.functional as F

# 1. Load model
def load_model():
    from torchvision.models import ResNet18_Weights
    model = models.resnet18(weights=ResNet18_Weights.DEFAULT)
    model.eval()
    model.to("cuda")
    return model

# 2. Preprocess image
def preprocess(image_path):
    preprocess_transform = transforms.Compose([
        transforms.Resize((224, 224)),
        transforms.ToTensor(),
        transforms.Normalize(
            mean=[0.485, 0.456, 0.406],
            std=[0.229, 0.224, 0.225]
        )
    ])
    image = Image.open(image_path).convert("RGB")
    input_tensor = preprocess_transform(image).unsqueeze(0)
    return input_tensor.to("cuda")

# 3. Postprocess: Apply softmax and map to class names
def postprocess(output, top_k=5):
    # Apply softmax to convert logits to probabilities
    probabilities = F.softmax(output[0], dim=0)

    # Load ImageNet class labels
    with open("imagenet_classes.txt", "r") as f:
        labels = [line.strip() for line in f.readlines()]

    # Get top K results
    top_probs, top_indices = torch.topk(probabilities, top_k)
    results = [(labels[idx], prob.item()) for idx, prob in zip(top_indices, top_probs)]
    return results

# 4. Inference
def infer(model, input_tensor):
    with torch.no_grad():
        output = model(input_tensor)  # Raw output logits
    return output.cpu()

# Main execution
if __name__ == "__main__":
    # Load model
    model = load_model()

    # Preprocess image
    input_tensor = preprocess("example.jpg")  # Replace with your image path

    # Perform inference
    output = infer(model, input_tensor)

    # Postprocess results
    top_k_results = postprocess(output, top_k=5)

    # Print top K results
    for label, prob in top_k_results:
        print(f"{label}: {prob:.4f}")

