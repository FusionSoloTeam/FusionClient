#include "gif_loader.h"

#include <base/log.h>
#include <base/mem.h>

#include <engine/gfx/image_loader.h>

#include <vector>

void CGifAnimation::Free()
{
	m_vFrames.clear();
	m_Width = 0;
	m_Height = 0;
}

namespace {

constexpr int GIF_MAX_DIMENSION = 8192;
constexpr size_t GIF_MAX_FRAMES = 512;
constexpr size_t GIF_MAX_TOTAL_PIXELS = 256ull * 1024 * 1024;
constexpr int GIF_MIN_FRAME_DELAY_MS = 20;
constexpr int GIF_DEFAULT_FRAME_DELAY_MS = 100;

class CGifReader
{
	const uint8_t *m_pData;
	size_t m_Size;
	size_t m_Pos = 0;

public:
	CGifReader(const uint8_t *pData, size_t Size) :
		m_pData(pData), m_Size(Size) {}

	size_t Pos() const { return m_Pos; }
	size_t Remaining() const { return m_Size - m_Pos; }

	bool ReadByte(uint8_t &Out)
	{
		if(m_Pos >= m_Size)
			return false;
		Out = m_pData[m_Pos++];
		return true;
	}

	bool ReadWord(uint16_t &Out)
	{
		uint8_t Low, High;
		if(!ReadByte(Low) || !ReadByte(High))
			return false;
		Out = (uint16_t)(Low | (High << 8));
		return true;
	}

	bool ReadBytes(void *pOut, size_t Count)
	{
		if(m_Pos + Count > m_Size)
			return false;
		mem_copy(pOut, m_pData + m_Pos, Count);
		m_Pos += Count;
		return true;
	}

	bool Skip(size_t Count)
	{
		if(m_Pos + Count > m_Size)
			return false;
		m_Pos += Count;
		return true;
	}

	bool ReadSubBlocks(std::vector<uint8_t> &vOut)
	{
		while(true)
		{
			uint8_t BlockSize;
			if(!ReadByte(BlockSize))
				return false;
			if(BlockSize == 0)
				return true;
			if(m_Pos + BlockSize > m_Size)
				return false;
			vOut.insert(vOut.end(), m_pData + m_Pos, m_pData + m_Pos + BlockSize);
			m_Pos += BlockSize;
		}
	}
};

struct SGifColorTable
{
	std::vector<uint8_t> m_vColors;

	void Resize(int Entries) { m_vColors.assign((size_t)Entries * 3, 0); }
	int NumEntries() const { return (int)(m_vColors.size() / 3); }

	void Get(int Index, uint8_t &R, uint8_t &G, uint8_t &B) const
	{
		const size_t Offset = (size_t)Index * 3;
		R = m_vColors[Offset + 0];
		G = m_vColors[Offset + 1];
		B = m_vColors[Offset + 2];
	}
};

bool GifLzwDecode(const std::vector<uint8_t> &vData, int MinCodeSize, size_t MaxPixels, std::vector<uint8_t> &vOut)
{
	if(MinCodeSize < 1 || MinCodeSize > 11)
		return false;

	const int ClearCode = 1 << MinCodeSize;
	const int EndCode = ClearCode + 1;
	constexpr int MAX_CODES = 4096;

	std::vector<uint16_t> vPrefix((size_t)MAX_CODES, 0);
	std::vector<uint8_t> vSuffix((size_t)MAX_CODES, 0);
	std::vector<uint8_t> vStack((size_t)MAX_CODES + 1, 0);

	for(int i = 0; i < ClearCode; ++i)
		vSuffix[(size_t)i] = (uint8_t)i;

	int CodeSize = MinCodeSize + 1;
	int CodeMask = (1 << CodeSize) - 1;
	int NextCode = EndCode + 1;
	int OldCode = -1;
	uint8_t FirstChar = 0;

	uint32_t BitBuffer = 0;
	int BitCount = 0;
	size_t BytePos = 0;

	vOut.clear();
	vOut.reserve(MaxPixels);

	while(true)
	{
		while(BitCount < CodeSize)
		{
			if(BytePos >= vData.size())
				return vOut.size() >= MaxPixels;
			BitBuffer |= (uint32_t)vData[BytePos++] << BitCount;
			BitCount += 8;
		}

		const int RawCode = (int)(BitBuffer & (uint32_t)CodeMask);
		BitBuffer >>= CodeSize;
		BitCount -= CodeSize;
		int Code = RawCode;

		if(Code == ClearCode)
		{
			CodeSize = MinCodeSize + 1;
			CodeMask = (1 << CodeSize) - 1;
			NextCode = EndCode + 1;
			OldCode = -1;
			continue;
		}

		if(Code == EndCode)
			break;

		if(OldCode < 0)
		{
			if(Code >= ClearCode)
				return false;
			if(vOut.size() >= MaxPixels)
				break;
			vOut.push_back(vSuffix[(size_t)Code]);
			FirstChar = vSuffix[(size_t)Code];
			OldCode = Code;
			continue;
		}

		const int InCode = Code;
		int StackSize = 0;

		if(Code >= NextCode)
		{
			vStack[(size_t)StackSize++] = FirstChar;
			Code = OldCode;
		}

		while(Code > EndCode)
		{
			if(StackSize >= MAX_CODES)
				return false;
			vStack[(size_t)StackSize++] = vSuffix[(size_t)Code];
			Code = vPrefix[(size_t)Code];
		}

		FirstChar = vSuffix[(size_t)Code];
		vStack[(size_t)StackSize++] = FirstChar;

		if(NextCode < MAX_CODES)
		{
			vPrefix[(size_t)NextCode] = (uint16_t)OldCode;
			vSuffix[(size_t)NextCode] = FirstChar;
			NextCode++;
			if(NextCode == (1 << CodeSize) && CodeSize < 12)
			{
				CodeSize++;
				CodeMask = (1 << CodeSize) - 1;
			}
		}

		OldCode = InCode;

		while(StackSize > 0)
		{
			if(vOut.size() >= MaxPixels)
				return true;
			vOut.push_back(vStack[(size_t)--StackSize]);
		}
	}

	return !vOut.empty();
}

class CGifParser
{
	const char *m_pContextName;
	CGifAnimation &m_Animation;

	size_t m_Width = 0;
	size_t m_Height = 0;
	SGifColorTable m_GlobalColorTable;
	std::vector<uint8_t> m_vCanvas;
	std::vector<uint8_t> m_vSavedCanvas;
	bool m_HasSavedCanvas = false;
	uint8_t m_aBackgroundColor[3] = {0, 0, 0};
	bool m_BackgroundKnown = false;
	bool m_CanvasOpaqueBackground = false;

	int m_Disposal = 0;
	int m_TransparentIndex = -1;
	bool m_HasTransparency = false;
	int m_DelayCs = 0;
	size_t m_FrameLeft = 0;
	size_t m_FrameTop = 0;
	size_t m_FrameWidth = 0;
	size_t m_FrameHeight = 0;
	int m_PrevDisposal = 0;
	size_t m_PrevLeft = 0;
	size_t m_PrevTop = 0;
	size_t m_PrevWidth = 0;
	size_t m_PrevHeight = 0;
	size_t m_TotalPixels = 0;

	void ResetGraphicControl()
	{
		m_Disposal = 0;
		m_TransparentIndex = -1;
		m_HasTransparency = false;
		m_DelayCs = 0;
	}

	bool ReadColorTable(CGifReader &Reader, int Entries, SGifColorTable &Table)
	{
		if(Entries <= 0 || Entries > 256)
			return false;
		Table.Resize(Entries);
		return Reader.ReadBytes(Table.m_vColors.data(), Table.m_vColors.size());
	}

	bool ClearRect(size_t Left, size_t Top, size_t Width, size_t Height)
	{
		if(Width == 0 || Height == 0)
			return true;
		for(size_t y = 0; y < Height; ++y)
		{
			uint8_t *pRow = m_vCanvas.data() + ((Top + y) * m_Width + Left) * 4;
			if(!m_CanvasOpaqueBackground)
			{
				mem_zero(pRow, Width * 4);
				continue;
			}
			for(size_t x = 0; x < Width; ++x)
			{
				pRow[x * 4 + 0] = m_aBackgroundColor[0];
				pRow[x * 4 + 1] = m_aBackgroundColor[1];
				pRow[x * 4 + 2] = m_aBackgroundColor[2];
				pRow[x * 4 + 3] = 255;
			}
		}
		return true;
	}

	void EnsureCanvas()
	{
		if(!m_vCanvas.empty())
			return;

		m_vCanvas.assign(m_Width * m_Height * 4, 0);
		m_vSavedCanvas.assign(m_Width * m_Height * 4, 0);
		m_CanvasOpaqueBackground = !m_HasTransparency && m_BackgroundKnown;
		if(!m_CanvasOpaqueBackground)
			return;

		for(size_t i = 0; i < m_Width * m_Height; ++i)
		{
			m_vCanvas[i * 4 + 0] = m_aBackgroundColor[0];
			m_vCanvas[i * 4 + 1] = m_aBackgroundColor[1];
			m_vCanvas[i * 4 + 2] = m_aBackgroundColor[2];
			m_vCanvas[i * 4 + 3] = 255;
		}
	}

	bool ApplyPreviousDisposal()
	{
		if(m_PrevDisposal == 2)
		{
			ClearRect(m_PrevLeft, m_PrevTop, m_PrevWidth, m_PrevHeight);
		}
		else if(m_PrevDisposal == 3 && m_HasSavedCanvas)
		{
			mem_copy(m_vCanvas.data(), m_vSavedCanvas.data(), m_vCanvas.size());
		}
		return true;
	}

	bool DrawFrame(const std::vector<uint8_t> &vIndices, const SGifColorTable &ColorTable, bool Interlaced)
	{
		if(m_FrameLeft + m_FrameWidth > m_Width || m_FrameTop + m_FrameHeight > m_Height)
			return false;

		size_t Index = 0;
		const size_t NumIndices = vIndices.size();
		const int NumColors = ColorTable.NumEntries();

		auto DrawRow = [&](size_t Row) {
			if(Row >= m_FrameHeight)
				return;
			uint8_t *pRow = m_vCanvas.data() + ((m_FrameTop + Row) * m_Width + m_FrameLeft) * 4;
			for(size_t x = 0; x < m_FrameWidth; ++x)
			{
				if(Index >= NumIndices)
					return;
				const int ColorIndex = vIndices[Index++];
				if(m_HasTransparency && ColorIndex == m_TransparentIndex)
					continue;
				if(ColorIndex >= NumColors)
					continue;
				uint8_t R, G, B;
				ColorTable.Get(ColorIndex, R, G, B);
				pRow[x * 4 + 0] = R;
				pRow[x * 4 + 1] = G;
				pRow[x * 4 + 2] = B;
				pRow[x * 4 + 3] = 255;
			}
		};

		if(Interlaced)
		{
			static const int STARTS[] = {0, 4, 2, 1};
			static const int STEPS[] = {8, 8, 4, 2};
			for(int Pass = 0; Pass < 4; ++Pass)
			{
				for(size_t Row = (size_t)STARTS[Pass]; Row < m_FrameHeight; Row += (size_t)STEPS[Pass])
					DrawRow(Row);
			}
		}
		else
		{
			for(size_t Row = 0; Row < m_FrameHeight; ++Row)
				DrawRow(Row);
		}

		return true;
	}

	bool PushFrame()
	{
		if(m_Animation.m_vFrames.size() >= GIF_MAX_FRAMES)
			return false;

		m_TotalPixels += m_Width * m_Height;
		if(m_TotalPixels > GIF_MAX_TOTAL_PIXELS)
		{
			log_warn("gif", "'%s' has too many frames, animation truncated", m_pContextName);
			return false;
		}

		SGifFrame Frame;
		Frame.m_Image.m_Width = m_Width;
		Frame.m_Image.m_Height = m_Height;
		Frame.m_Image.m_Format = CImageInfo::FORMAT_RGBA;
		Frame.m_Image.Allocate();
		mem_copy(Frame.m_Image.m_pData, m_vCanvas.data(), m_vCanvas.size());

		int DelayMs = m_DelayCs * 10;
		if(DelayMs <= 0)
			DelayMs = GIF_DEFAULT_FRAME_DELAY_MS;
		if(DelayMs < GIF_MIN_FRAME_DELAY_MS)
			DelayMs = GIF_MIN_FRAME_DELAY_MS;
		Frame.m_DelayMs = DelayMs;

		m_Animation.m_vFrames.push_back(std::move(Frame));
		return true;
	}

public:
	CGifParser(const char *pContextName, CGifAnimation &Animation) :
		m_pContextName(pContextName), m_Animation(Animation) {}

	bool Parse(CGifReader &Reader)
	{
		uint8_t aSignature[6];
		if(!Reader.ReadBytes(aSignature, sizeof(aSignature)))
			return false;
		const bool Is87a = mem_comp(aSignature, "GIF87a", 6) == 0;
		const bool Is89a = mem_comp(aSignature, "GIF89a", 6) == 0;
		if(!Is87a && !Is89a)
		{
			log_error("gif", "'%s' is not a GIF file", m_pContextName);
			return false;
		}

		uint16_t ScreenWidth, ScreenHeight;
		uint8_t Packed, BackgroundIndex, AspectRatio;
		if(!Reader.ReadWord(ScreenWidth) || !Reader.ReadWord(ScreenHeight) ||
			!Reader.ReadByte(Packed) || !Reader.ReadByte(BackgroundIndex) || !Reader.ReadByte(AspectRatio))
			return false;

		if(ScreenWidth == 0 || ScreenHeight == 0 || ScreenWidth > GIF_MAX_DIMENSION || ScreenHeight > GIF_MAX_DIMENSION)
		{
			log_error("gif", "'%s' has unsupported dimensions %ux%u", m_pContextName, ScreenWidth, ScreenHeight);
			return false;
		}

		m_Width = ScreenWidth;
		m_Height = ScreenHeight;

		if((Packed & 0x80) != 0)
		{
			const int Entries = 2 << (Packed & 0x07);
			if(!ReadColorTable(Reader, Entries, m_GlobalColorTable))
				return false;

			if((int)BackgroundIndex < m_GlobalColorTable.NumEntries())
			{
				m_GlobalColorTable.Get((int)BackgroundIndex, m_aBackgroundColor[0], m_aBackgroundColor[1], m_aBackgroundColor[2]);
				m_BackgroundKnown = true;
			}
		}


		while(true)
		{
			uint8_t Introducer;
			if(!Reader.ReadByte(Introducer))
				break;

			if(Introducer == 0x3B)
				break;

			if(Introducer == 0x21)
			{
				uint8_t Label;
				if(!Reader.ReadByte(Label))
					return false;

				if(Label == 0xF9)
				{
					uint8_t BlockSize;
					if(!Reader.ReadByte(BlockSize))
						return false;
					if(BlockSize < 4)
					{
						if(!Reader.Skip(BlockSize))
							return false;
						std::vector<uint8_t> vDummy;
						if(!Reader.ReadSubBlocks(vDummy))
							return false;
						continue;
					}

					uint8_t ControlPacked, DelayLow, DelayHigh, TransparentIndex;
					if(!Reader.ReadByte(ControlPacked) || !Reader.ReadByte(DelayLow) || !Reader.ReadByte(DelayHigh) ||
						!Reader.ReadByte(TransparentIndex))
						return false;

					m_Disposal = (ControlPacked >> 2) & 0x07;
					m_DelayCs = (int)(DelayLow | (DelayHigh << 8));
					m_HasTransparency = (ControlPacked & 0x01) != 0;
					m_TransparentIndex = m_HasTransparency ? (int)TransparentIndex : -1;

					if(BlockSize > 4 && !Reader.Skip(BlockSize - 4))
						return false;

					std::vector<uint8_t> vDummy;
					if(!Reader.ReadSubBlocks(vDummy))
						return false;
					continue;
				}

				std::vector<uint8_t> vSkip;
				if(!Reader.ReadSubBlocks(vSkip))
					return false;
				continue;
			}

			if(Introducer != 0x2C)
			{
				log_error("gif", "'%s' contains an unknown block (0x%02x)", m_pContextName, Introducer);
				return false;
			}

			uint16_t FrameLeft, FrameTop, FrameWidth, FrameHeight;
			uint8_t FramePacked;
			if(!Reader.ReadWord(FrameLeft) || !Reader.ReadWord(FrameTop) ||
				!Reader.ReadWord(FrameWidth) || !Reader.ReadWord(FrameHeight) || !Reader.ReadByte(FramePacked))
				return false;

			if(FrameWidth == 0 || FrameHeight == 0)
			{
				ResetGraphicControl();
				continue;
			}

			const bool HasLocalColorTable = (FramePacked & 0x80) != 0;
			const bool Interlaced = (FramePacked & 0x40) != 0;

			SGifColorTable LocalColorTable;
			const SGifColorTable *pColorTable = &m_GlobalColorTable;

			if(HasLocalColorTable)
			{
				const int Entries = 2 << (FramePacked & 0x07);
				if(!ReadColorTable(Reader, Entries, LocalColorTable))
					return false;
				pColorTable = &LocalColorTable;
			}

			if(pColorTable->NumEntries() == 0)
			{
				log_error("gif", "'%s' is missing a color table", m_pContextName);
				return false;
			}

			uint8_t MinCodeSize;
			if(!Reader.ReadByte(MinCodeSize))
				return false;

			std::vector<uint8_t> vLzwData;
			if(!Reader.ReadSubBlocks(vLzwData))
				return false;

			EnsureCanvas();

			if(!ApplyPreviousDisposal())
				return false;

			if(m_Disposal == 3)
			{
				mem_copy(m_vSavedCanvas.data(), m_vCanvas.data(), m_vCanvas.size());
				m_HasSavedCanvas = true;
			}

			m_FrameLeft = FrameLeft;
			m_FrameTop = FrameTop;
			m_FrameWidth = FrameWidth;
			m_FrameHeight = FrameHeight;

			std::vector<uint8_t> vIndices;
			if(!GifLzwDecode(vLzwData, (int)MinCodeSize, (size_t)FrameWidth * FrameHeight, vIndices))
			{
				log_error("gif", "'%s' contains invalid LZW data", m_pContextName);
				return false;
			}

			if(!DrawFrame(vIndices, *pColorTable, Interlaced))
				return false;

			if(!PushFrame())
				break;

			m_PrevDisposal = m_Disposal;
			m_PrevLeft = FrameLeft;
			m_PrevTop = FrameTop;
			m_PrevWidth = FrameWidth;
			m_PrevHeight = FrameHeight;

			ResetGraphicControl();
		}

		if(m_Animation.m_vFrames.empty())
		{
			log_error("gif", "'%s' contains no frames", m_pContextName);
			return false;
		}

		m_Animation.m_Width = m_Width;
		m_Animation.m_Height = m_Height;
		return true;
	}
};

}

bool CGifLoader::Load(const uint8_t *pData, size_t DataSize, const char *pContextName, CGifAnimation &Animation)
{
	Animation.Free();
	if(pData == nullptr || DataSize == 0)
		return false;

	CGifReader Reader(pData, DataSize);
	CGifParser Parser(pContextName, Animation);
	if(!Parser.Parse(Reader))
	{
		Animation.Free();
		return false;
	}

	return true;
}

