from dataset import EventDatase
from torch.utils.data import DataLoader

def train(args):
    dataset = EventDataset('..\\Data')
    dataloader = DataLoader(dataset, batch_size=1, shuffle=True, num_workers=8)
