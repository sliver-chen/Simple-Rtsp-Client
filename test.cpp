#include "RtspPlayer.hpp"

using namespace RK;

int main(int argc, char **argv) {
	RtspPlayer::Ptr player = std::make_shared<RtspPlayer>();
    player->Play("rtsp://184.72.239.149/vod/mp4://BigBuckBunny_175k.mov");

	getchar();
	return 0;
}
